
#include "pch.h"
#include <iostream>
#include <random>
#include <amp.h>
#include "perfomance.h"

using namespace std;
using namespace concurrency;

// Initialize the resolution of the timer
LARGE_INTEGER Timer::m_freq = \
(QueryPerformanceFrequency(&Timer::m_freq), Timer::m_freq);

// Calculate the overhead of the timer
LONGLONG Timer::m_overhead = Timer::GetOverhead();

// CONST
int const
element_count = 10,
tileSize = 2;

// Global vars
float aMatrix[element_count];
float bMatrix[element_count];
float cMatrix[element_count];
float dMatrix[element_count];
array_view<float, 1> a(element_count, aMatrix);
array_view<float, 1> b(element_count, bMatrix);
array_view<float, 1> c(element_count, cMatrix);
array_view<float, 1> d(element_count, dMatrix);

float rand_n(int max) {
	return rand() % max + 1; //рандом от 1 до MAX
}


int print(array_view<float, 1> a) {
	for (int i = 0; i < a.extent[0]; i++)
		//for (int j = 0; j < C; j++)

			// Prints ' ' if j != n-1 else prints '\n'           
			cout << a[i] << " ";
	cout << "\n";

	return 0;
}

void AmpReductionSimple(array_view<float, 1> a) {

	// Если четный массив и прибавляем к сумме последний элемент
	float tail_sum = (element_count % 2) ? a[element_count - 1] : 0;
	// Помещаем сумму в массив по ссылке
	array_view<float, 1> av_tail_sum(1, &tail_sum);

	/* редукция tileSize
	  С каждой итерацией мы уменьшаем наш массив в 2 раза
	  [1,2,3,4,5,6] превращается в [1+4,2+5,3+6,4,5,6]
	  в av_tail_sum[0] суммируется только половина массива.
	  */
	for (unsigned s = element_count / tileSize; s > 0; s /= tileSize)
	{
		parallel_for_each(a.extent, [=](index<1> idx) restrict(amp)
		{
			a[idx] = a[idx] + a[idx + s];

			if ((idx[0] == s - 1) && (s & 0x1) && (s != 1))
			{
				av_tail_sum[0] += a[s - 1];
			}
		});
	}

	// Copy the results back to CPU.
	std::vector<float> result(1);
	copy(a.section(0, 1), result.begin());
	av_tail_sum.synchronize();

	// print result
	// cout << "Result: " << result[0] + tail_sum << "\n";
}

void AmpReductionWindow(array_view<float, 1> a) {
	float tail_sum = 0;
	array_view<float, 1> av_tail_sum(1, &tail_sum);

	// запоминаем предыдущий stride
	int prev_s = element_count;

	// опять же с помощью редукции делим массив пополам, пока делится больше 0
	for (int s = element_count / tileSize; s > 0; s /= tileSize)
	{
		parallel_for_each(a.extent, [=](index<1> idx) restrict(amp)
		{
			float sum = 0.f;
			for (int i = 0; i < tileSize; i++)
			{
				sum += a[idx + i * s];
			}
			a[idx] = sum;

			if ((idx[0] == s - 1) && ((s % tileSize) != 0) && (s > tileSize))
			{
				for (int i = ((s - 1) / tileSize) * tileSize; i < s; i++)
				{
					av_tail_sum[0] += a[i];
				}
			}
		});
		prev_s = s;
	}

	std::vector<float> result(prev_s);
	// в результат также помещаем последний stride ( в av_tail_sum он не помещается, т.к. последний )
	copy(a.section(0, prev_s), result.begin());
	av_tail_sum.synchronize();

	// Тут суммируем результат
	// cout << "Result window: " << (av_tail_sum[0]) + (result[0]) << "\n";
}


void AmpReductionTiledDecomposition(array_view<float, 1> av_src) {
	int element_count_var = element_count;

	float arr_2[element_count / tileSize];

	// array_views may be swapped after each iteration.
	array_view<float, 1> av_dst(arr_2);
	av_dst.discard_data();

	// Reduce using parallel_for_each as long as the sequence length
	// is evenly divisable to the number of threads in the tile.
	while ((element_count_var % tileSize) == 0)
	{
		parallel_for_each(av_src.extent.tile<tileSize>(), [=](tiled_index<tileSize> tidx) restrict(amp)
		{
			// Use tile_static as a scratchpad memory.
			tile_static float tile_data[tileSize];

			unsigned local_idx = tidx.local[0];
			tile_data[local_idx] = av_src[tidx.global];
			tidx.barrier.wait();

			// Reduce within a tile using multiple threads.
			for (unsigned s = 1; s < tileSize; s *= 2)
			{
				if (local_idx % (2 * s) == 0)
				{
					tile_data[local_idx] += tile_data[local_idx + s];
				}

				tidx.barrier.wait();
			}

			// Store the tile result in the global memory.
			if (local_idx == 0)
			{
				av_dst[tidx.tile] = tile_data[0];
			}
		});

		// Update the sequence length, swap source with destination.
		element_count_var /= tileSize;
		std::swap(av_src, av_dst);
		av_dst.discard_data();
	}

	// Perform any remaining reduction on the CPU.
	//std::vector<float> result(element_count_var);
	//copy(av_src.section(0, element_count_var), result.begin());

	//cout << "Result decomposition: " << result[0] + result[1] + result[2] << "\n";
}

void AmpReductionTiledCascade(array_view<float, 1> a) {
	int tilesCount = element_count / tileSize;

	int stride = tileSize * tilesCount * 2;

	parallel_for_each(a.extent.tile<tileSize>(), [=](tiled_index<tileSize> tidx) restrict(amp)
	{
		// Use tile_static as a scratchpad memory.
		tile_static float tile_data[tileSize];

		int local_idx = tidx.local[0];

		// Reduce data strides of twice the tile size into tile_static memory.
		int input_idx = (tidx.tile[0] * 2 * tileSize) + local_idx;
		tile_data[local_idx] = 0;
		do
		{
			tile_data[local_idx] += a[input_idx] + a[input_idx + tileSize];
			input_idx += stride;
		} while (input_idx < element_count);

		tidx.barrier.wait();

		for (int stride = tileSize / 2; stride > 0; stride /= 2)
		{
			if (local_idx < stride)
			{
				tile_data[local_idx] += tile_data[local_idx + stride];
			}

			tidx.barrier.wait();
		}
	});
}


void startReductionSimple() {
	AmpReductionSimple(a);
}
void startReductionWindow() {
	AmpReductionWindow(b);
}
void startReductionDecomposition() {
	AmpReductionTiledDecomposition(c);
}
void startReductionCascade() {
	AmpReductionTiledCascade(d);
}

void main()
{
	for (int i = 0;i < element_count;i++) {
		aMatrix[i] = bMatrix[i] = cMatrix[i] = dMatrix[i] = rand_n(10);
	}

	PerfomanceTest timer = PerfomanceTest();

	cout << "AMP Reduction tests:\n";
	cout << "Reduction          :  " << timer.Start(startReductionSimple) << "(ms)\n";
	cout << "Reduction Window   :  " << timer.Start(startReductionWindow) << "(ms)\n";
	cout << "Reduction tiled    :  " << timer.Start(startReductionDecomposition) << "(ms)\n";
	cout << "Reduction cascade  :  " << timer.Start(startReductionCascade) << "(ms)\n";
}
