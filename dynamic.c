#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <math.h>
#include <string.h>

#define 	FILE_NAME  					"com.txt"
#define 	V  		 					1134900
#define 	E			  					2987604
#define 	row_miss_stall			  	8
#define   parallelism 					8
#define	no_of_col_per_row		1024
#define   v_per_read					8	
#define 	queue							128
						
typedef struct {
  unsigned int s;
  unsigned int d;
} edge;

void sort_edge_set_in_queue(edge *array,int left,int right){
	int i=left,j=right;
	edge tmp;
	edge pivot =array[(left+right)/2];
	while(i<=j){
	    while(array[i].d<pivot.d)
	        i++;
	    while(array[j].d>pivot.d)
	        j--;
	    if(i<=j){
	        tmp = array[i];
	        array[i] = array[j];
	        array[j] = tmp;
	        i++; j--;
	    }
	}
	if(left<j)
	    sort_edge_set_in_queue(array,left,j);
	if(i<right)
	    sort_edge_set_in_queue(array,i,right);	
}

void	sort_edge_set(edge *array,int left,int right_i){
	int right = right_i - right_i%queue;
	int i;
	for(i=0; i<right/queue; i=i+1){
		sort_edge_set_in_queue(array, i*queue, (i+1)*queue-1) ;
	}
}


int main(int argc, char ** argv){
		FILE *fp;
		int i,j,k,h,count, m;
		int u1, u2;
		int total_clk=0;
		int no_of_partitions=(int)V/16/1024+2;
		int no_of_row_hit=0;
		int no_of_row_miss=0;
		
		edge* 		edge_set [no_of_partitions]; 
		int 		no_of_edges  	[no_of_partitions]; 
		int 		no_of_updates  	[no_of_partitions]; 
		int 		no_of_updates_read_gather [no_of_partitions]; 
		
		for (i=0; i<no_of_partitions; i++){
			no_of_edges[i]=0;	
			no_of_updates[i]=0;	
			no_of_updates_read_gather[i]=0;
			edge_set[i] = (edge*) malloc(sizeof(edge) *1000000); // this number should be larger than 'max'
		}		
		
		if ((fp=fopen(FILE_NAME,"r"))==NULL)
			printf("Cannot open file. Check the name.\n"); 
		else {
    			for(i=0;i<E;i++){
				if(fscanf(fp,"%d %d\n",&u1,&u2)!=EOF){
					j=(int)(u1/16/1024);
					//if(j>=no_of_partitions) printf("%d",no_of_partitions);
					edge_set[j][no_of_edges[j]].s=u1;			
					edge_set[j][no_of_edges[j]].d=u2;					
					no_of_edges[j]++;					
				}
			}
			fclose(fp);
		}
		int max = 0;
		for (i=0; i<no_of_partitions; i++){						
			if(no_of_edges[i]>max)
				max = no_of_edges[i];				
		}			
		printf("%d\n", max);
		
		for (i=0; i<no_of_partitions; i++){	
			sort_edge_set(edge_set[i], 0, no_of_edges[i]-1);
		}	
		// count how many updates per partition produced in gather & read in scatter
		for (i=0; i<no_of_partitions; i++){	
			for(j=0; j<no_of_edges[i]-no_of_edges[i]%parallelism; j=j+parallelism){				
				h=(int)(edge_set[i][j].d/16/1024);
				no_of_updates[i]++;
				no_of_updates_read_gather[h]++;
				for(k=1; k<parallelism; k++){
					if(edge_set[i][j+k].d != edge_set[i][j+k-1].d){
						no_of_updates[i]++;
						h=(int)(edge_set[i][j+k].d/16/1024);					
						no_of_updates_read_gather[h]++;						
					}
				}
			}
		}		
		//for (i=0; i<no_of_partitions; i++) printf("%d %d %d %d\n", i, no_of_updates_read_gather[i], no_of_updates[i], no_of_edges[i]); // check how many edges in each partition
		//for (i=0; i<no_of_edges[146]; i++) printf("%d %d %d\n", i, edge_set[146][i].s, edge_set[146][i].d);
		
		//Scatter 
		for(i=0; i<no_of_partitions; i++){
			no_of_row_miss++;			
			//load vertices	
			for(j=0; j<16*1024; j=j+v_per_read){
				if(j%no_of_col_per_row==0){
					no_of_row_miss++;			
				}	
				else{
					no_of_row_hit++;			
				}	
			}	
			no_of_row_miss++;		
			// stream in edges
			//read
			for(j=0; j<no_of_edges[i]-no_of_edges[i]%8; j=j+8){
				if(j%no_of_col_per_row==0)
					no_of_row_miss++;			
				else
					no_of_row_hit++;			
			}
			// extra stalls due to shift read to write						
			m = (int) edge_set[i][0].d/16/1024;
			for(j=1; j<no_of_edges[i]; j++){
				h=(int) edge_set[i][j].d/16/1024;
				if(m!=h){
					no_of_row_miss=no_of_row_miss+3;
					m=h;
				}						
			}							
			no_of_row_miss += no_of_updates[i]/1024;
		}	
													
		//gather
		for(i=0; i<no_of_partitions; i++){
			no_of_row_miss ++;
			//load vertices	
			for(j=0; j<16*1024; j=j+v_per_read){
				if(j%no_of_col_per_row==0){
					no_of_row_miss++;			
				}	
				else{
					no_of_row_hit++;			
				}	
			}	
			no_of_row_miss++;	
			
			//stream in updates	
			for(j=0; j<no_of_updates_read_gather[i]; j=j+8){
				if(j%no_of_col_per_row==0)
					no_of_row_miss++;			
				else
					no_of_row_hit++;	
			}	
			no_of_row_miss++;	
			//write out vertices	
			for(j=0; j<16*1024; j=j+v_per_read){
				if(j%no_of_col_per_row==0)
					no_of_row_miss++;
				else
					no_of_row_hit++;	
			}
		}		
		total_clk=row_miss_stall*no_of_row_miss+no_of_row_hit;
		//printf("%d %d\n", no_of_row_miss, no_of_row_miss+no_of_row_hit); 
		double hit_rate = (double) no_of_row_hit/(no_of_row_miss+no_of_row_hit);
		printf("total_clk is %d in time %d ns\nhit rate is %lf\nthroughput is %lf GTEPS\n", total_clk, total_clk*5, hit_rate, (double)E/total_clk/5); 
		
}
