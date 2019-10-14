#include "SudokuEstimator.h"
#include <chrono>

#include "AC3.h"

struct Results {
	std::mutex mutex;

	Big_Integer  sum = 0;
	unsigned int n   = 0;
} results;

void SudokuEstimator::backtrack_with_forward_check() {
	int current_x = traverser.x;
	int current_y = traverser.y;
	
	assert(sudoku.get(current_x, current_y) == 0);

	int domain[Sudoku<N>::size];
	int domain_size = sudoku.get_domain(current_x, current_y, domain);

	assert(domain_size == sudoku.domain_sizes[Sudoku<N>::get_index(current_x, current_y)]);

	// Try all possible values for the cell at (x, y)
	for (int i = 0 ; i < domain_size; i++) {
		int value = domain[i];
		
		// Try the current value
		if (sudoku.set_with_forward_check(current_x, current_y, value)) {
			if (traverser.move(&sudoku)) {
				// If the Sudoku was completed by this move, add 1 to the solution count
				backtrack += 1;
				
				assert(sudoku.is_valid_solution());
			} else {
				// Otherwise, count the solutions that include this move
				backtrack_with_forward_check();
			} 
		}

		traverser.x = current_x;
		traverser.y = current_y;

		sudoku.reset_cell(current_x, current_y);
	}
}

void SudokuEstimator::knuth() {
	estimate = 1;

	int domain[Sudoku<N>::size];

	for (int i = 0; i < s; i++) {
		int coordinate = coordinates[i];
		int x = coordinate & 0x0000ffff;
		int y = coordinate >> 16;

		assert(y % N != 0); // First row of every block is filled with numbers from a Latin Rectangle
		assert(sudoku.get(x, y) == 0); // Cell should be empty
		
		int domain_size = sudoku.get_domain(x, y, domain);

		if (domain_size == 0) {
			estimate = 0;
			
			return;
		}

		estimate *= domain_size;

		std::uniform_int_distribution<int> distribution(0, domain_size - 1);
		int random_value_from_domain = domain[distribution(rng)];

		// Use forward checking for a possible early out
		// If any domain becomes empty the Sudoku can't be completed and 0 can be returned.
		if (!sudoku.set_with_forward_check(x, y, random_value_from_domain)) {
			estimate = 0;
			
			return;
		}
	}
}

void SudokuEstimator::estimate_solution_count() {
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

	// Repeat until a valid Latin Rectangle is obtained
	retry: {
		// Randomly shuffle every row but the first one
		for (int row = 1; row < N; row++) {
			std::shuffle(rows[row], rows[row] + Sudoku<N>::size, rng);
		}

		// Check if the current permutation of rows is a Latin Rectangle
		for (int row = 1; row < N; row++) {
			for (int i = 0; i < Sudoku<N>::size; i++) {
				for (int j = 0; j < row; j++) {
					if (rows[row][i] == rows[j][i]) {
						// Not a valid Latin Rectangle, retry
						goto retry;
					}
				}
			}
		}
	}

	// Fill every Nth row of the Sudoku with a row from the Latin Rectangle
	for (int row = 0; row < N; row++) {
		for (int i = 0; i < Sudoku<N>::size; i++) {
			bool domains_valid = sudoku.set_with_forward_check(i, row * N, rows[row][i]);

			assert(domains_valid);
		}
	}

	// Select s random cells from the other rows.
	std::shuffle(coordinates, coordinates + coordinate_count, rng);

	// Estime using Knuth's algorithm
	knuth();

	if (mpz_is_zero(estimate)) return;

	// Reduce domain sizes using AC3
	// If a domain was made empty, return false
	if (!ac3(&sudoku)) {
		estimate = 0;

		return;
	}

	traverser.seek_first(&sudoku);

	// Count all Sudoku solutions that contain the current configuration as a subset
	backtrack = 0;
	backtrack_with_forward_check();
	
	if (mpz_is_zero(backtrack)) {
		estimate = 0;

		return;
	}

	// Multiply our estimate, the exact amount of backtracking solutions and a constant
	estimate *= backtrack;
}

void SudokuEstimator::run() {
	assert(s < coordinate_count);
	assert(Sudoku<N>::size < (1 << 16)); // Coordinate indices (x, y) are packed together into a 32bit integer

	rng = std::mt19937(random_device());

	int index = 0;
	for (int j = 1; j < Sudoku<N>::size; j++) {
		if (j % N == 0) continue;

		for (int i = 0; i < Sudoku<N>::size; i++) {
			coordinates[index++] = j << 16 | i;
		}
	}

	Big_Integer  batch_sum;
	unsigned int batch_size = 100;

	while (true) {
		batch_sum = 0;

		// Sum 'batch_size' estimations, then store the result
		for (unsigned int i = 0; i < batch_size; i++) {
			estimate_solution_count();

			batch_sum += estimate;
		}

		results.mutex.lock();
		{
			results.sum += batch_sum;
			results.n   += batch_size;
		}
		results.mutex.unlock();
	}
}

void report_results() {
	// Amount of N x N^2 Latin Rectangles, this constant can be used to speed
	// up the process of estimating the amount of valid N^2 x N^2 Sudoku Grids
	Big_Integer latin_rectangle_count; 

	const char * true_value;
	switch (Sudoku<N>::size) {
		case 4: {
			true_value = "288";

			latin_rectangle_count = 24 * 9; // 4! times number of 2x4 latin rectangles where the first row is 1 .. 4
		} break;
		case 9: {
			true_value = "6670903752021072936960"; 
			
			latin_rectangle_count = 362880 * 5792853248; // 9! times number of 3x9 latin rectangles where the first row is 1 .. 9
		} break;
		case 16: {
			true_value = "595840000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000"; // Estimate, real value is currently unknown

			latin_rectangle_count  = 20922789888000;	// 16!
			latin_rectangle_count *= 1307674368000;		// Convert from number of Reduced Latin Rectangles to actual number
			latin_rectangle_count /= 479001600;

			// Number of reduced Latin Rectangles
			latin_rectangle_count *= 1 << 14;
			latin_rectangle_count *= 243 * 2693;
			latin_rectangle_count *= 42787;
			latin_rectangle_count *= 1699482467;
			latin_rectangle_count *= 8098773443;
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

	Big_Integer  results_sum;
	Big_Integer  results_avg;
	unsigned int results_n;

	while (true) {
		using namespace std::chrono_literals;

		std::this_thread::sleep_for(1s);

		results.mutex.lock();
		{
			results_sum = results.sum;
			results_n   = results.n;
		}
		results.mutex.unlock();

		if (results_n > 0) { // @PERFORMANCE
			results_avg = (results_sum / results_n) * latin_rectangle_count;

			//printf(  "%llu: Est: ",     results_n); mpz_out_str(stdout, 10, estimate.__get_mp());
			printf(  "%u: Avg: ",     results_n); mpz_out_str(stdout, 10, results_avg.__get_mp());
			printf("\n%u: Tru: %s\n", results_n,  true_value);
		}
	}
}