#include <unistd.h>
#include "yaffut.h"

#include <vector>

class Partitioner
{
public:
	std::vector<unsigned int> function_masks;
	std::vector<unsigned int> solution;
	
	int solve(unsigned int available_mask)
	{
		solution.resize(function_masks.size());
		return recurse(0, available_mask);
	}

protected:
	int recurse(unsigned int function_index, unsigned int mask)
	{
		if (function_index >= function_masks.size())
			return 0;
		for (unsigned int index = 0, index_bit = 1;
			 mask >= index_bit;
			 ++index, index_bit <<= 1)
		{
			if (mask & index_bit)
			{
				if (function_masks[function_index] & index_bit)
				{
					int r = recurse(function_index+1, mask & ~index_bit);
					if (r >= 0)
					{
						solution[function_index] = index;
					}
					return r;
				}
			}
		}
		return -1;
	}
};

class WeightedPartitioner
{
public:
	std::vector<unsigned int> function_masks;
	std::vector<unsigned int> function_scores;
	std::vector<int> solution;
	
	unsigned int solve(unsigned int available_mask)
	{
		solution.assign(function_masks.size(), -1);
		best_score.assign(function_masks.size(), 0);
		return recurse(0, available_mask, 0);
	}
	
	unsigned int calc_max_attainable_score(unsigned int function_index, unsigned int mask)
	{
		unsigned int result = 0;
		while (function_index < function_masks.size())
		{
			if (function_masks[function_index] & mask)
				result += function_scores[function_index];
			++function_index;
		}
		return result;
	}
	
	void check_valid()
	{
		const unsigned int num_functions = solution.size();
		EQUAL(num_functions, function_masks.size());
		for (unsigned int i = 0; i < num_functions-1; ++i)
			for (unsigned int j=i+1; j < num_functions; ++j)
				CHECK(solution[i] != solution[j]);
	}

protected:
	std::vector<unsigned int> best_score;

	unsigned int recurse(unsigned int function_index, unsigned int mask, unsigned int score)
	{
		if (function_index >= function_masks.size())
			return score;
		const unsigned int max_attainable_score = calc_max_attainable_score(function_index, mask);
		unsigned int max_score = score;
		int local_solution = -1;
		for (unsigned int index = 0, index_bit = 1;
			 (mask >= index_bit) && index_bit && ((max_score - score) < max_attainable_score);
			 ++index, index_bit <<= 1)
		{
			if (mask & index_bit)
			{
				if (function_masks[function_index] & index_bit)
				{
					unsigned int r = recurse(function_index+1, mask & ~index_bit, score + function_scores[function_index]);
					if (r > max_score)
					{
						local_solution = index;
						max_score = r;
					}
				}
			}
		}
		/* What if we do NOT pick this function */
		if ((max_score - score) < max_attainable_score)
		{
			unsigned int r = recurse(function_index+1, mask, score);
			if (r > max_score)
			{
				local_solution = -1;
				max_score = r;
			}
		}
		
		if (max_score > best_score[function_index])
		{
			best_score[function_index] = max_score;
			solution[function_index] = local_solution;
		}
	
		return max_score;
	}
};

FUNC(solve_partition_configuration)
{
	Partitioner p;
	
	p.function_masks.push_back(0b110);
	p.function_masks.push_back(0b011);
	p.function_masks.push_back(0b110);
	
	YAFFUT_CHECK(p.solve(0b111) >= 0);
	EQUAL(1u, p.solution[0]);
	EQUAL(0u, p.solution[1]);
	EQUAL(2u, p.solution[2]);

	p.function_masks[0] = 0b100;
	p.function_masks[1] = 0b010;
	p.function_masks[2] = 0b111;
	YAFFUT_CHECK(p.solve(0b111) >= 0);
	EQUAL(2u, p.solution[0]);
	EQUAL(1u, p.solution[1]);
	EQUAL(0u, p.solution[2]);

	unsigned int items = 31;
	p.function_masks.resize(items);
	for (unsigned int i=0; i<items; ++i)
		p.function_masks[i] = (1 << items) - 1;
	YAFFUT_CHECK(p.solve((1 << items) - 1) >= 0);
	for (unsigned int i=0; i<items; ++i)
		EQUAL(i, p.solution[i]);

	p.function_masks.resize(items);
	for (unsigned int i=0; i<items; ++i)
		p.function_masks[i] = (1 << i);
	YAFFUT_CHECK(p.solve((1 << items) - 1) >= 0);
	for (unsigned int i=0; i<items; ++i)
		EQUAL(i, p.solution[i]);

	/* No solution possible */
	p.function_masks.resize(3);
	p.function_masks[0] = 0b100;
	p.function_masks[1] = 0b010;
	p.function_masks[2] = 0b100;
	YAFFUT_CHECK(p.solve(0b1111) < 0);
}

FUNC(solve_partition_configuration_with_mask)
{
	Partitioner p;
	p.function_masks.push_back(0b110110);
	p.function_masks.push_back(0b110011);
	p.function_masks.push_back(0b110110);
	/* No solution if partition 0 is in use */
	YAFFUT_CHECK(p.solve(0b1110) < 0);

	p.function_masks[0] = 0b1101;
	p.function_masks[1] = 0b0111;
	p.function_masks[2] = 0b1101;
	YAFFUT_CHECK(p.solve(0b1110) >= 0); /* Eliminate bit 0 */
	EQUAL(2u, p.solution[0]);
	EQUAL(1u, p.solution[1]);
	EQUAL(3u, p.solution[2]);
	YAFFUT_CHECK(p.solve(0b1011) >= 0); /* Eliminate bit 2 */
	EQUAL(0u, p.solution[0]);
	EQUAL(1u, p.solution[1]);
	EQUAL(3u, p.solution[2]);
}

FUNC(solve_partition_configuration_with_weight)
{
	WeightedPartitioner p;
	p.function_masks.push_back(0b001);
	p.function_masks.push_back(0b010);
	p.function_masks.push_back(0b100);
	p.function_scores.push_back(1);
	p.function_scores.push_back(2);
	p.function_scores.push_back(3);
	
	EQUAL(3u, p.calc_max_attainable_score(0, 0b011));
	EQUAL(5u, p.calc_max_attainable_score(0, 0b110));
	EQUAL(5u, p.calc_max_attainable_score(1, 0b110));
	EQUAL(2u, p.calc_max_attainable_score(1, 0b011));
	
	unsigned int r = p.solve(0b111);
	EQUAL(6U, p.calc_max_attainable_score(0, 0b111));
	EQUAL(5U, p.calc_max_attainable_score(1, 0b111));
	EQUAL(3U, p.calc_max_attainable_score(2, 0b111));
	EQUAL(6u, r);
	EQUAL(0, p.solution[0]);
	EQUAL(1, p.solution[1]);
	EQUAL(2, p.solution[2]);
	p.check_valid();
	p.function_masks[0] = 0b001;
	p.function_masks[1] = 0b010;
	p.function_masks[2] = 0b010;
	EQUAL(4u, p.solve(0b111));
	EQUAL(0, p.solution[0]);
	EQUAL(-1, p.solution[1]);
	EQUAL(1, p.solution[2]);
	p.check_valid();

	p.function_masks[0] = 0b011;
	p.function_masks[1] = 0b011;
	p.function_masks[2] = 0b011;
	EQUAL(5u, p.solve(0b011));
	EQUAL(-1, p.solution[0]);
	EQUAL(0, p.solution[1]);
	EQUAL(1, p.solution[2]);
	p.check_valid();
	
	EQUAL(6u, p.calc_max_attainable_score(0, 0b011));

	/* Create a large search space. The actual test here is that this
	 * finished in reasonable time. With the initial implementation without
	 * score estimations, the runtime on an i7 is several hours... */
	const unsigned int functions = 8;
	const unsigned int partitions = 30;
	p.function_masks.assign(functions, (1 << partitions) - 1);
	p.function_scores.assign(functions, 1);
	EQUAL(std::min(functions, partitions), p.solve((1 << partitions) - 1));
	p.check_valid();
	
	p.function_masks[functions-1] = 0;
	EQUAL(std::min(functions-1, partitions), p.solve((1 << partitions) - 1));
	p.check_valid();
}
