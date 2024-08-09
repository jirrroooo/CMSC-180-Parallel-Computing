# CMSC 180 - T3L
# Laboratory Exercise 2
# Programmer:       John Rommel B. Octavo
# Student Number:   202*-*****

import random
import time

def pearson_cor(X, y, m, n):
    v = [0] * n
    
    # Calculate sums
    sum_X = [sum(row) for row in X]
    sum_y = sum(y)
    sum_X_squared = [sum(item ** 2 for item in row) for row in X]
    sum_y_squared = sum(val ** 2 for val in y)
    
    x_mul_y = []
    
    for row in X:
        temp = []
        for i in range(n):
            temp.append(row[i]*y[i])
        x_mul_y.append(temp)
    
    sum_Xy = [sum(row) for row in x_mul_y]
    
    # Calculate Pearson Correlation Coefficient vector
    for j in range(n):
        numerator = m * sum_Xy[j] - sum_X[j] * sum_y
        denominator = ((m * sum_X_squared[j] - sum_X[j] ** 2) * (m * sum_y_squared - sum_y ** 2)) ** 0.5
        
        
        v[j] = numerator / denominator
    
    return v

def main():
    n = int(input("Enter the size of the square matrix and the length of the vector: "))
    
    X = [[random.randint(1, 100) for _ in range(n)] for _ in range(n)]
    y = [random.randint(1, 100) for _ in range(n)]
    

    time_before = time.time()
    v = pearson_cor(X, y, n, n)
    time_after = time.time()
    
    time_elapsed = time_after - time_before
    
    print("Pearson correlation coefficient vector:", v)
    print("Time elapsed:", time_elapsed, "seconds")
   
if __name__ == "__main__":
    main()