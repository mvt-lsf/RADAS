#include <math_functions.h>

float *moving1avg_full(float *vector, unsigned int vector_size, unsigned int window, float *vector_avg)
{
    int i;
    int j;
    float sum_i = 0.0;
    if (window > 1)
    {
        for (i = 0; i < vector_size - window + 1; ++i)
        {
            if (i == 0)
            {
                for (j = 0; j < window; ++j)
                {
                    sum_i += vector[j];
                }
            }
            else
            {
                sum_i += vector[i + window - 1] - vector[i - 1];
            }
            vector_avg[i] = sum_i / window;
        }
        j = 1;
        for (i = vector_size - window + 1; i < vector_size; ++i)
        {
            sum_i -= vector[i - 1];
            vector_avg[i] = sum_i / (window - j);
            j++;
        }
    }
    else
    {
        for (i = 0; i < vector_size; ++i)
        {
            vector_avg[i] = vector[i];
        }
    }
}

float *moving1avg(float *vector, unsigned int vector_size, unsigned int window, float *vector_avg, unsigned int vector_avg_size)
{

    int i;
    int j;
    float sum_i = 0.0;
    if (window > 1)
    {
        for (i = 0; i < vector_avg_size; ++i)
        {
            if (i == 0)
            {
                for (j = 0; j < window; ++j)
                {
                    sum_i += vector[j];
                }
            }
            else
            {
                sum_i += vector[i + window - 1] - vector[i - 1];
            }
            vector_avg[i] = sum_i / window;
        }
    }
    else
    {
        for (i = 0; i < vector_avg_size; ++i)
        {
            vector_avg[i] = vector[i];
        }
    }
}
