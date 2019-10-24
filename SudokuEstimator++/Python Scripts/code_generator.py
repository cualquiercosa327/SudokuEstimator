N = 4
M = 4
size = N * M

def update_constraint_set(i, j):
    index = i + j * size
    return '\tvalid &= (domain_sizes[{}] -= (constraints[{} + value]++ == 0)) != 0;\n'.format(index, index * size)

def update_constraint_reset(i, j):
    index = i + j * size
    return '\tdomain_sizes[{}] += (constraints[{} + value]-- == 1);\n'.format(index, index * size)

def output_update_domains_set(file, x, y):
    bx = M * int(x / M)
    by = N * int(y / N)
    bxe = bx + M
    bye = by + N

    file.write('bool Generated::update_domains_set_{}_{}(int domain_sizes[], int constraints[], int value) {{\n'.format(x, y))
    file.write('\tbool valid = true;\n')

    file.write('\n')
    file.write('\t// Update current row\n')

    for i in range(0,   x):    file.write(update_constraint_set(i, y))
    for i in range(x+1, size): file.write(update_constraint_set(i, y))

    file.write('\n')
    file.write('\t// Update current column\n')

    for j in range(0, y):    
        if j % N != 0: file.write(update_constraint_set(x, j))
    for j in range(y+1, size):
        if j % N != 0: file.write(update_constraint_set(x, j))

    file.write('\n')
    file.write('\t// Update current block\n')

    for j in range(by+1, y):
        for i in range(bx,  x):   file.write(update_constraint_set(i, j))
        for i in range(x+1, bxe): file.write(update_constraint_set(i, j))
    for j in range(y+1, bye):
        for i in range(bx,  x):   file.write(update_constraint_set(i, j))
        for i in range(x+1, bxe): file.write(update_constraint_set(i, j))

    file.write('\n')
    file.write('\treturn valid;\n')
    file.write('}\n\n')

def output_update_domains_reset(file, x, y):
    bx = M * int(x / M)
    by = N * int(y / N)
    bxe = bx + M
    bye = by + N

    file.write('void Generated::update_domains_reset_{}_{}(int domain_sizes[], int constraints[], int value) {{\n'.format(x, y))
    file.write('\t// Update current row\n')

    for i in range(0,   x):    file.write(update_constraint_reset(i, y))
    for i in range(x+1, size): file.write(update_constraint_reset(i, y))

    file.write('\n')
    file.write('\t// Update current column\n')

    for j in range(0, y): 
        if j % N != 0: file.write(update_constraint_reset(x, j))
    for j in range(y+1, size):
        if j % N != 0: file.write(update_constraint_reset(x, j))

    file.write('\n')
    file.write('\t// Update current block\n')

    for j in range(by+1, y):
        for i in range(bx,  x):   file.write(update_constraint_reset(i, j))
        for i in range(x+1, bxe): file.write(update_constraint_reset(i, j))
    for j in range(y+1, bye):
        for i in range(bx,  x):   file.write(update_constraint_reset(i, j))
        for i in range(x+1, bxe): file.write(update_constraint_reset(i, j))

    file.write('}\n\n')

# Output the header file
with open('../Generated.h', 'w') as file:
    file.write('''// File generated by Python script, do not modify

typedef bool (* SetFunction)  (int domain_sizes[], int constraints[], int value);
typedef void (* ResetFunction)(int domain_sizes[], int constraints[], int value);

struct Generated {{
inline static constexpr int N = {N}; 
inline static constexpr int M = {M};

'''.format(N=N, M=M, size=size*size))

    for y in range(0, size):
        for x in range(0, size):
            file.write('static bool update_domains_set_{}_{}  (int domain_sizes[], int constraints[], int value);\n'.format(x, y, N, M))
            file.write('static void update_domains_reset_{}_{}(int domain_sizes[], int constraints[], int value);\n'.format(x, y, N, M))
            file.write('\n')

    file.write('inline static constexpr const SetFunction table_set[{}] = {{\n'.format(size * size))
    for y in range(0, size):
        for x in range(0, size):
            file.write('\tupdate_domains_set_{}_{},\n'.format(x, y))
    file.write('};\n')
    file.write('inline static constexpr const ResetFunction table_reset[{}] = {{\n'.format(size * size))
    for y in range(0, size):
        for x in range(0, size):
            file.write('\tupdate_domains_reset_{}_{},\n'.format(x, y))
    file.write('};\n')
    file.write('};\n')

# Output the implementation file
with open('../Generated.cpp', 'w') as file:
    file.write('// File generated by Python script, do not modify\n#include "Generated.h"\n\n')

    for y in range(0, size):
        for x in range(0, size):
            output_update_domains_set(file, x, y)
            output_update_domains_reset(file, x, y)

print('File written successfully!')