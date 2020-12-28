
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
R = 8,
C = 8,
tileSize = 4;

//GLOBAL VARS
float aMatrix[R*C];
float bMatrix[R*C];
array_view<float, 2> a(R, C, aMatrix);
array_view<float, 2> b(R, C, bMatrix);

float rand_n(int max) {
	return rand() % max + 1; //рандом от 1 до MAX
}


int print(array_view<float, 2> a) {
	for (int i = 0; i < R; i++)
		for (int j = 0; j < C; j++)

			// Prints ' ' if j != n-1 else prints '\n'           
			cout << a[i][j] << " \n"[j == C - 1];

	return 0;
}


void AmpBlockMatrixTranspose(array_view<float, 2> a) {
	float transposedata[R*C];

	array_view<float, 2> transposed(C, R, transposedata);

	transposed.discard_data();

	parallel_for_each(
		a.extent.tile<tileSize, tileSize>(),
		[=](tiled_index<tileSize, tileSize> t_idx) restrict(amp) {
		// Заносим в память ГПУ
		// От туда могут получать информацию другие потоки
		tile_static float localData[tileSize][tileSize];

		// Заносим туда текущее значение
		localData[t_idx.local[1]][t_idx.local[0]] = a[t_idx.global];

		// ждём, пока все потоки пройдутся
		t_idx.barrier.wait();

		// outIdx - грубо говоря массив x,y но он сам вычисляет позицию в одномерном массиве
		index<2> outIdx(index<2>(t_idx.tile_origin[1], t_idx.tile_origin[0]) + t_idx.local);

		transposed[outIdx] = localData[t_idx.local[0]][t_idx.local[1]];
	});
}

// В данном случае у нас кол-во потоков = кол-во строк
// при первом прохождений N потоков запишут в память все числа x=0
// Далее на y=0 выполняем умножения после занесения всех в память (после barrier.wait)
void AmpBlockGlobalMatrixMultipleDecomposition(array_view<float, 2> a, array_view<float, 2> b) {
	float Result[R*C];

	array_view<float, 2> product(C, R, Result);
	product.discard_data();

	// Дабы "типо" проходить по этому массиву
	float a_matrix[R];
	array_view<float, 1> a_m(R, a_matrix);

	// Каждую строка - новый parallel_for_each
	for (int row = 0;row < R;row++ ) {

		// Параллелизм проходит только ширине массива
		parallel_for_each(
			a_m.extent,
			[=](index<1> idx) restrict(amp)
		{
			//Обнуляем переменную, чтобы дальше делать +=
			product[row][idx[0]] = 0;
			for (int j = 0; j < b.extent[0]; j++) {
				product[row][idx[0]] += a(row, j) * b(j, idx[0]);
			}
		});
	}

	// Синхронизация, как я понял, не обязательно, но обычно её пишут :/
	product.synchronize();
}

void AmpBlockGlobalMatrixMultipleShared(array_view<float, 2> a, array_view<float, 2> b) {
	float Result[R*C];

	array_view<float, 2> product(C, R, Result);

	parallel_for_each(product.extent.tile<tileSize, tileSize>(),
		[=](tiled_index<tileSize, tileSize> t_idx) restrict(amp)
	{
		int row = t_idx.local[0];
		int col = t_idx.local[1];
		int rowGlobal = t_idx.global[0];
		int colGlobal = t_idx.global[1];
		float sum = 0;

		// Т.к. мы здесь проходим по блокам, а не по всему массиву, то нужно пройти блоками несколько раз.
		for (int i = 0; i < C; i += tileSize) {
			// СМОТРИ AmpBlockMatrixTranspose
			tile_static float locA[tileSize][tileSize];
			tile_static float locB[tileSize][tileSize];
			locA[row][col] = a(rowGlobal, col + i);
			locB[row][col] = b(row + i, colGlobal);
			t_idx.barrier.wait();

			// Опять же суммируем
			for (int k = 0; k < tileSize; k++) {
				sum += locA[row][k] * locB[k][col];
			}

			// На всякий случай.
			t_idx.barrier.wait();
		}

		product[t_idx.global] = sum;
	});

	product.synchronize();
}

// Without Shared memory (not tile_static), only global
void AmpBlockGlobalMatrixMultiple(array_view<float, 2> a, array_view<float, 2> b) {
	float Result[R*C];

	array_view<float, 2> product(C, R, Result);

	parallel_for_each(product.extent.tile<tileSize, tileSize>(),
		[=](tiled_index<tileSize, tileSize> t_idx) restrict(amp)
	{
		int row = t_idx.global[0];
		int col = t_idx.global[1];
		float sum = 0;

		for (int i = 0; i < b.extent[0]; i++) {
			t_idx.barrier.wait();
			sum += a(row, i) * b(i, col);
		}
		product[t_idx.global] = sum;
	});

	product.synchronize();
}

//lol kek :)
void startTranspose() {
	AmpBlockMatrixTranspose(a);
}
void startGlobalMem() {
	AmpBlockGlobalMatrixMultiple(a, b);
}
void startSharedMem() {
	AmpBlockGlobalMatrixMultipleShared(a, b);
}
void startDecomposition() {
	AmpBlockGlobalMatrixMultipleDecomposition(a, b);
}

int main()
{
	for (int i = 0;i < R*C;i++) {
		aMatrix[i] = rand_n(10);
		bMatrix[i] = rand_n(10);
	}

	PerfomanceTest timer = PerfomanceTest();

	cout << "AMP Matrix Transpose                :  " << timer.Start(startTranspose) << "(ms)\n";
	cout << "AMP Matrix multiple (Global Memory) :  " << timer.Start(startGlobalMem) << "(ms)\n";
	cout << "AMP Matrix multiple (Shared Memory) :  " << timer.Start(startSharedMem) << "(ms)\n";
	cout << "AMP Matrix multiple (Decomposition) :  " << timer.Start(startDecomposition) << "(ms)\n";
}