
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <stdint.h>
#include <stdlib.h>




double randn (uint32_t mu, uint32_t sigma)
{
	double U1, U2, W, mult; //double
	static double X1, X2; //double
	static uint8_t call = 0; //int

	if (call == 1)
	{
		call = !call;
		return (mu + sigma * (uint32_t) X2);
    }
 
	do
	{
		
		U1 = -1 + ((double) rand () / RAND_MAX) * 2;
		U2 = -1 + ((double) rand () / RAND_MAX) * 2;
		W = pow (U1, 2) + pow (U2, 2);
	}
	while (W >= 1 || W == 0);
	mult = sqrt ((-2 * log (W)) / W);
	X1 = U1 * mult;
	X2 = U2 * mult;

	call = !call;
 
	return (uint32_t)(mu + sigma * (double) X1);
}

uint32_t solar_energy_input (uint32_t time_solar, uint8_t scalling_factor)
{
	uint32_t result;
	uint32_t normal_var;
	int solar_mu = 200;
	int solar_sigma = 40;

	normal_var = randn(solar_mu, solar_sigma);
	result = scalling_factor * normal_var * cos( time_solar / (70 * M_PI) ) * cos( time_solar / (100 * M_PI) );
	return result;
}

int main( int argc, char *argv[] )
{
	uint32_t solar_harv;
	uint32_t iterations;
	int i;
	if ( argc > 2 )
	{
		printf(">> Error, too many arguments\n");
	}
	else
	{
		srand ( time(NULL) );
		FILE *fp;
		fp = fopen("solar_model.txt", "w");
		if ( fp == NULL )
		{
			fclose(fp);
			printf(">> Error while opening file\n");
		}
		else
		{
			iterations = atoi(argv[1]);
			printf("Iteration: %d\n", iterations);
			for ( i=0; i<(int)iterations; i++ )
			{
				// printf("Before solar_energy_input\n");
				do
				{
					solar_harv = solar_energy_input(iterations, 2);
				}while(solar_harv < 0 || solar_harv > 700);

				// if (solar_harv >= 0 && solar_harv < 500){}
				fprintf(fp, "%lu\n",solar_harv);
				
			}
			
			fclose(fp);
		}
	}
	return 0;
}