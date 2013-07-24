#include <unistd.h>
#include "yaffut.h"

#include <vector>

class Partitioner
{
public:
	std::vector<int> function_masks;
	std::vector<int> solution;
	
	int recurse(int function_index, unsigned int mask)
	{
		if (function_index >= function_masks.size())
			return 0;
		for (unsigned int index = 0, index_bit = 1;
			 mask >= index_bit /*index < number_of_partitions*/ ;
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
	
	int solve(unsigned int available_mask)
	{
		solution.resize(function_masks.size());
		return recurse(0, available_mask);
	}
};


FUNC(solve_partition_configuration)
{
	Partitioner p;
	
	p.function_masks.push_back(0b110);
	p.function_masks.push_back(0b011);
	p.function_masks.push_back(0b110);
	
	YAFFUT_CHECK(p.solve(0b111) >= 0);
	EQUAL(1, p.solution[0]);
	EQUAL(0, p.solution[1]);
	EQUAL(2, p.solution[2]);

	p.function_masks[0] = 0b100;
	p.function_masks[1] = 0b010;
	p.function_masks[2] = 0b111;
	YAFFUT_CHECK(p.solve(0b111) >= 0);
	EQUAL(2, p.solution[0]);
	EQUAL(1, p.solution[1]);
	EQUAL(0, p.solution[2]);

	int items = 31;
	p.function_masks.resize(items);
	for (int i=0; i<items; ++i)
		p.function_masks[i] = (1 << items) - 1;
	YAFFUT_CHECK(p.solve((1 << items) - 1) >= 0);
	for (int i=0; i<items; ++i)
		EQUAL(i, p.solution[i]);

	p.function_masks.resize(items);
	for (int i=0; i<items; ++i)
		p.function_masks[i] = (1 << i);
	YAFFUT_CHECK(p.solve((1 << items) - 1) >= 0);
	for (int i=0; i<items; ++i)
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
	YAFFUT_CHECK(p.solve(0xb1110) < 0);

	p.function_masks[0] = 0b1101;
	p.function_masks[1] = 0b0111;
	p.function_masks[2] = 0b1101;
	YAFFUT_CHECK(p.solve(0b1110) >= 0); /* Eliminate bit 0 */
	EQUAL(2, p.solution[0]);
	EQUAL(1, p.solution[1]);
	EQUAL(3, p.solution[2]);
	YAFFUT_CHECK(p.solve(0b1011) >= 0); /* Eliminate bit 2 */
	EQUAL(0, p.solution[0]);
	EQUAL(1, p.solution[1]);
	EQUAL(3, p.solution[2]);
}
