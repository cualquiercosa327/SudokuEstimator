#include <iostream>

#include "ScopedTimer.h"

#include "AC3.h"

#include "DomainSudoku.h"
#include "SudokuTraverser.h"

//#include "BigInteger.h"
#include "BigInt.h"

constexpr int s = 6;

constexpr int N = 3;
Domain_Sudoku<N> sudoku; // N^2 x N^2 Sudoku

BigInt latin_rectangle_count;

//Row_Right_Column_Down_Traverser<N> traverser; 
//Domain_Sorted_Traverser<N> traverser; 
Most_Constrained_Traverser<N> traverser;

constexpr int coordinate_count = Sudoku<N>::size * (Sudoku<N>::size - N);
int coordinates[coordinate_count];

template<typename Sudoku_Traverser, int N>
BigInt backtrack_with_forward_check(Domain_Sudoku<N> * sudoku, Sudoku_Traverser * traverser) {
	int current_x = traverser->x;
	int current_y = traverser->y;
	
	assert(sudoku->get(current_x, current_y) == 0);

	int domain[Domain_Sudoku<N>::size];
	int domain_size = sudoku->get_domain(current_x, current_y, domain);

	assert(domain_size == sudoku->domain_sizes[current_x + current_y * Domain_Sudoku<N>::size]);

	BigInt total = 0;

    // Try all possible values for the cell at (x, y)
    for (int i = 0 ; i < domain_size; i++) {
		int value = domain[i];
		
        // Try the current value
		if (sudoku->set_with_forward_check(current_x, current_y, value)) {
			if (traverser->move(sudoku)) {
				// If the Sudoku was completed by this move, add 1 to the solution count
				total += 1;

				assert(sudoku->is_valid_solution());
			} else {
				// Otherwise, count the solutions that include this move
				total += backtrack_with_forward_check(sudoku, traverser);
			} 
		}

		traverser->x = current_x;
		traverser->y = current_y;

        sudoku->set_with_forward_check(current_x, current_y, 0);
    }

    return total;
}

// Takes a random walk of length s through the tree of all possible Sudokus
BigInt knuth() {
    BigInt D = 1ULL;

	int domain[Sudoku<N>::size];

    for (int i = 0; i < s; i++) {
		int coordinate = coordinates[i];

        int x = coordinate & 0x0000ffff;
        int y = coordinate >> 16;

		assert(y % N != 0);

        int domain_size = sudoku.get_domain(x, y, domain);

        if (domain_size == 0) return 0;

        D *= domain_size;

		int random_value_from_domain = domain[rand() % domain_size];

        // Use forward checking for a possible early out
		// If any domain becomes empty the Sudoku can't be completed and 0 can be returned.
        if (!sudoku.set_with_forward_check(x, y, random_value_from_domain)) return 0;
    }

    return D;
}

BigInt estimate_solution_count() {
	// Reset all cells to 0 and clear domains
    sudoku.reset();

	// Fill every Nth row with a row from a random N x N^2 Latin Rectangle
	// The first row is always 1 .. N^2
	int rows[N][Sudoku<N>::size];

	// Initialize each row of the Latin Rectangle with the numbers 1 .. N^2
	for (int row = 0; row < N; row++) {
		for (int i = 0; i < Sudoku<N>::size; i++) {
			rows[row][i] = i + 1;
		}
	}

	// @PERFORMANCE: Chance of this succeeding is too low to be fast
	// Repeat until a valid Latin Rectangle is obtained
	bool valid;
	do {
		// Randomly shuffle every row but the first one
		for (int row = 1; row < N; row++) {
			std::random_shuffle(rows[row], rows[row] + Sudoku<N>::size);
		}

		// Check if the current permutation of rows is a Latin Rectangle
		valid = true;
		for (int row = 1; row < N; row++) {
			for (int i = 0; i < Sudoku<N>::size; i++) {
				for (int j = 0; j < row; j++) {
					if (rows[row][i] == rows[j][i]) {
						// Not a valid Latin Rectangle, retry
						valid = false;
						
						break;
					}
				}
			}
		}
	} while(!valid);

	// Fill every Nth row of the Sudoku with a row from the Latin Rectangle
	for (int row = 0; row < N; row++) {
		for (int i = 0; i < Sudoku<N>::size; i++) {
			bool domains_valid = sudoku.set_with_forward_check(i, row * N, rows[row][i]);

			assert(domains_valid);
		}
	}

    // Select s random cells from the other rows.
	std::random_shuffle(coordinates, coordinates + coordinate_count);

    // Estime using Knuth's algorithm
    BigInt estimate = knuth();

    if (estimate.is_zero()) return estimate;

	// Reduce domain sizes using AC3
    ac3(&sudoku);

	traverser.seek_first(&sudoku);

	// Count all Sudoku solutions that contain the current configuration as a subset
	BigInt backtrack = backtrack_with_forward_check(&sudoku, &traverser);
	
	if (backtrack.is_zero()) return backtrack;

    // Multiply our estimate, the exact amount of backtracking solutions and a constant
    return estimate * backtrack * latin_rectangle_count;
}

int main() {
	// Set random seed
	srand(time(NULL));

	assert(Sudoku<N>::size < (1 << 16));
	assert(s < coordinate_count);

	// Assert that it is possible to store the maximum estimate possible from Knuth's algorithm in an unsigned long long
	assert((unsigned long long)pow(N, 2 * s) < (unsigned long long)(-1));

	int index = 0;
	for (int j = 1; j < Sudoku<N>::size; j++) {
		if (j % N == 0) continue;

		for (int i = 0; i < Sudoku<N>::size; i++) {
			coordinates[index++] = j << 16 | i;
		}
	}

	const char * true_value;
	switch (Sudoku<N>::size) {
        case 4:  {
			true_value = "288";

			latin_rectangle_count = 24 * 9; // 4! times number of 2x4 latin rectangles where the first row is 1 .. 4
		} break;
        case 9: {
			true_value = "6670903752021072936960"; 
			
			latin_rectangle_count = BigInt(362880) * BigInt(5792853248); // 9! times number of 3x9 latin rectangles where the first row is 1 .. 9
		} break;
		case 16: {
			true_value = "595840000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000"; // Estimate, real value is currently unknown

			latin_rectangle_count = BigInt(20922789888000) * BigInt("6314083806394358817244705266941952"); // 16! times number of 4x16 latin rectangles where the first row is 1 .. 16
		} break;
		case 25: {
			true_value = "TODO"; // @TODO

			latin_rectangle_count = -1; // @TODO: http://combinatoricswiki.org/wiki/Enumeration_of_Latin_Squares_and_Rectangles
		} break;
        default: {
			true_value = "Unknown";
			
			latin_rectangle_count = -1;
		} break;
    }

	BigInt sum = 0;
	long long n = 0;

	long long duration_sum = 0;
	auto timer = std::chrono::high_resolution_clock::now();

	while (true) {
		BigInt estimate = estimate_solution_count();
		
		sum += estimate;
		n++;
        
		// Every 100 iterations print the current stats
		if (n % 100 == 0) {
			BigInt avg = sum / n;

			printf("%llu: Est: %s\n", n, estimate.to_string().c_str());
			printf("%llu: Avg: %s\n", n, avg.to_string().c_str());
			printf("%llu: Tru: %s\n", n, true_value);

			long long duration = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - timer).count();
			timer = std::chrono::high_resolution_clock::now();

			if (duration >= 1000000) {
				printf("Duration: %llu us (%llu s)\n", duration, duration / 1000000);
			} else if (duration >= 1000) {
				printf("Duration: %llu us (%llu ms)\n", duration, duration / 1000);
			} else {
				printf("Duration: %llu us\n", duration);
			}

			duration_sum += duration;
			printf("Avg duration: %llu\n", duration_sum / (n / 100));
		}
	}
}
