#include <test.h>

void fib(int n)
{
    if (n <= 0)
    {
        printf("The number of terms must be greater than 0.\n");
        return;
    }

    int a = 0, b = 1; // First two numbers of Fibonacci sequence

    printf("First %d terms of the Fibonacci sequence:\n", n);

    for (int i = 0; i < n; i++)
    {
        printf("%d ", a);

        // Compute the next number in the sequence
        int next = a + b;
        a = b;
        b = next;
    }

    printf("\n");
}

void triangle(int n)
{
    if (n <= 0)
    {
        printf("Number of rows must be greater than 0.\n");
        return;
    }

    int triangle[n][n]; // Array to store Pascal's Triangle values

    // Generate Pascal's Triangle
    for (int i = 0; i < n; i++)
    {
        for (int j = 0; j <= i; j++)
        {
            if (j == 0 || j == i)
            {
                triangle[i][j] = 1;
            }
            else
            {
                triangle[i][j] = triangle[i - 1][j - 1] + triangle[i - 1][j];
            }
        }
    }

    // Print Pascal's Triangle with padding
    for (int i = 0; i < n; i++)
    {
        // Add padding for alignment
        for (int space = 0; space < n - i - 1; space++)
        {
            printf("   "); 
        }

        // Print the numbers in the row
        for (int j = 0; j <= i; j++)
        {
            printf("%6d", triangle[i][j]); // Align numbers within 6 spaces
        }
        printf("\n");
    }
}