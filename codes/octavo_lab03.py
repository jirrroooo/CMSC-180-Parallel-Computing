# CMSC 180 - T3L
# Laboratory Exercise 3
# Programmer:       John Rommel B. Octavo
# Student Number:   202*-*****

import random
import time
import threading
import subprocess
import psutil

result = []
index = 0

def transpose_matrix(matrix):
    # Check if the matrix is empty
    if not matrix:
        return []

    # Number of rows and columns in the original matrix
    rows = len(matrix)
    cols = len(matrix[0])

    # Transpose the matrix
    transposed = [[row[i] for row in matrix] for i in range(cols)]

    return transposed

def pearson_cor(X, y, m, n):
    X = transpose_matrix(X)
    m = n
    n = len(X)
    
    v = [0] * n
    
    # Calculate sums
    sum_X = [sum(row) for row in X]
    sum_y = sum(y)
    sum_X_squared = [sum(item ** 2 for item in row) for row in X]
    sum_y_squared = sum(val ** 2 for val in y)
    
    x_mul_y = []
    
    for row in X:
        temp = []
        for i in range(m):
            temp.append(row[i]*y[i])
        x_mul_y.append(temp)
    
    sum_Xy = [sum(row) for row in x_mul_y]
    
    # Calculate Pearson Correlation Coefficient vector
    for j in range(n):
        numerator = m * sum_Xy[j] - sum_X[j] * sum_y
        denominator = ((m * sum_X_squared[j] - sum_X[j] ** 2) * (m * sum_y_squared - sum_y ** 2)) ** 0.5
        
        v[j] = numerator / denominator
    
    global result
    result[index] = v


def worker(submatrix, y, m, n):
    global index
    pearson_cor(submatrix, y, m, n)
    index += 1
    
def main():
    n = int(input("Enter the size of the square matrix and the length of the vector: "))
    t = int(input("Enter the number of threads: "))
    
    X = [[random.randint(1, 100) for _ in range(n)] for _ in range(n)]
    y = [random.randint(1, 100) for _ in range(n)]
    
    submatrices = []
    
    # Divide X into t submatrices
    submatrix_size = n // t
    ctr = 1
    ctr2 = 0
    for i in range(t):
        if(t > (n // 2) and submatrix_size == 1 and ctr <= (n-t) ):
            start_row = i * 2
            end_row = (i + ctr + 1)
            submatrices.append(transpose_matrix(X[start_row:end_row]))
            ctr += 1
        elif(t > (n // 2) and submatrix_size == 1 and ctr > (n-t) ):
            ctr = ((n-t)*2) + ctr2
            start_row = ctr
            end_row = ctr + 1
            submatrices.append(transpose_matrix(X[start_row:end_row]))
            ctr2 += 1
        else:
            start_row = i * submatrix_size
            end_row = (i + 1) * submatrix_size if i != t - 1 else n
            submatrices.append(transpose_matrix(X[start_row:end_row]))
        
    global result
    global index
    result = [None] * t
    
    # Get available CPU cores
    cpu_cores = psutil.cpu_count(logical=False)
    
    threads = []
    
    time_before = time.time()
    
    for i in range(t):
        thread = threading.Thread(target=worker, args=(submatrices[i], y, len(submatrices[i]), n))
        threads.append(thread)
        thread.start()
    
    for thread in threads:
        thread.join()
    
    time_after = time.time()
    
    v = sum(result, [])

    time_elapsed = time_after - time_before
    
    print("Pearson correlation coefficient vector:", v)
    print("Time elapsed:", time_elapsed, "seconds")

if __name__ == "__main__":
    main()
