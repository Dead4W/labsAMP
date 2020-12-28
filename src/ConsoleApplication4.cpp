#include "pch.h"
#include<iostream>
#include<amp.h>
#include <random>
#include <ctime>

using namespace std;
using namespace concurrency;

// CppAmpMethod()
int const t_size = 5;

// AmpMatrixMultiplication()
int const N = 5,
R = 2,
C = 12;

int random_int(int max) {
	return rand() % max + 1; //рандом от 1 до 10
}

void CppAmpMethod() {
	//Источник: https://github.com/MicrosoftDocs/cpp-docs/blob/master/docs/parallel/amp/cpp-amp-overview.md
	int aCPP[] = { 1, 2, 3, 4, 5 };
	int bCPP[] = { 6, 7, 8, 9, 10 };
	int sumCPP[t_size];

	// AMP объекты для обмена информации
	array_view<const int, 1> a(t_size, aCPP);	// Это нужно для чтения в потоках и представления массива, как одномерный
	array_view<const int, 1> b(t_size, bCPP);	// Это нужно для чтения в потоках и представления массива, как одномерный
	array_view<int, 1> sum(t_size, sumCPP);		// А тут для записи результата (нету const)

	sum.discard_data();							// Оптимизация, преобразует объект под accelerator_view (используется, если существующие данные в переменной не нужны?!)

	parallel_for_each(
		sum.extent,								// Define the compute domain, which is the set of threads that are created.
		/*
		 * Define the code to run on each thread on the accelerator.
		 * index<2> и index<3> используются в двумерных и трёхмерных массивах соответственно
		 */
		[=](index<1> idx) restrict(amp) {
			sum[idx] = a[idx] + b[idx];
		}
	);

	wcout << "A {";
	for (int i = 0; i < t_size; i++) {
		if (i > 0) {
			wcout << ",";
		}
		wcout << aCPP[i];
	}
	wcout << "}" << endl;

	wcout << "B {";
	for (int i = 0; i < t_size; i++) {
		if (i > 0) {
			wcout << ",";
		}
		wcout << bCPP[i];
	}
	wcout << "}" << endl;

	wcout << "R {";
	for (int i = 0; i < t_size; i++) {
		if (i > 0) {
			wcout << ",";
		}
		wcout << sum(i);
	}
	wcout << "}";
}

void Matrix() {
	int multiNumber = 10;

	int aCPP[R*C];

	for (int i = 0; i < R*C;i++) {
		aCPP[i] = random_int(50);
	}

	/* 
	Так, array_view представляет обычный массив, как двумерный либо как трёхмерный
	Следовательно код снизу означает:
		<int, 2>								// 2D матрица
		a()										// В переменную "a"
		(2, 12, aCPP)							// 2x12 - размер матрицы, из переменной aCPP
	 */

	array_view<int, 2> a(2, 12, aCPP);			// 2D, 2x12 Matrix
	//array_view<int, 3> a(2, 12, 5, aCPP);		// 3D, 2x12x5 Matrix

	/*
	index<2>									// 2D Matrix
	idx(1, 4)									// индекс по координатам матрицы, где y=1;x=4
	*/
	index<2> idx(1, 4);

	// Далее просто вставляем индекс, при y=1;x=4, он будет равен idx=16 => a[idx] = 50
	std::cout << "value: " << a[idx] << endl;
	// Output: 50

}

void AmpMatrixMultiplication() {
	int aCPP[R*C];
	int sumCPP[R*C];

	for (int i = 0; i < R*C;i++) {
		aCPP[i] = random_int(50);
	}

	// AMP объекты для обмена информации
	array_view<const int, 2> a(R, C, aCPP);				// Это нужно для чтения в потоках и представления массива, как одномерный, представляем как константу!
	array_view<int, 2> sum(R, C, sumCPP);				// А тут для записи результата переменную функцию(нету const)

	sum.discard_data();									// Оптимизация, преобразует объект под accelerator_view (используется, если существующие данные в переменной не нужны?!)

	parallel_for_each(
		sum.extent,										// Массив, хранящий в себе количество строк и колон: [строк, колон]
		/*
		 * Define the code to run on each thread on the accelerator.
		 * index<2> и index<3> используются в двумерных и трёхмерных массивах соответственно
		 */
		[=](index<2> idx) restrict(amp) {
		sum[idx] = a[idx] * N;
	}
	);

	wcout << "Original matrix: " << endl;

	// Print the results.
	for (int i = 0; i < R; i++) {
		for (int j = 0; j < C; j++) {
			if (j > 0) {
				wcout << ",";
			}
			wcout << a(i, j);
		}
		wcout << endl;
	}

	wcout << endl << "Multipled matrix: " << endl;

	// Print the results.
	for (int i = 0; i < R; i++) {
		for (int j = 0; j < C; j++) {
			if (j > 0) {
				wcout << ",";
			}
			wcout << sum(i, j);
		}
		wcout << endl;
	}
}

void AmpMatrixMultipledByMatrix() {
	int aCPP[25];
	int bCPP[25];
	int sumCPP[25];

	for (int i = 0; i < 25;i++) {
		aCPP[i] = random_int(50);
	}
	for (int i = 0; i < 25;i++) {
		bCPP[i] = random_int(50);
	}

	// AMP объекты для обмена информации
	array_view<const int, 2> a(5, 5, aCPP);				// Это нужно для чтения в потоках и представления массива, как одномерный, представляем как константу!
	array_view<const int, 2> b(5, 5, bCPP);				// Это нужно для чтения в потоках и представления массива, как одномерный, представляем как константу!
	array_view<int, 2> sum(5, 5, sumCPP);				// А тут для записи результата переменную функцию(нету const)

	sum.discard_data();									// Оптимизация, преобразует объект под accelerator_view (используется, если существующие данные в переменной не нужны?!)

	parallel_for_each(
		sum.extent,										// Массив, хранящий в себе количество строк и колон: [строк, колон]
		/*
		 * Define the code to run on each thread on the accelerator.
		 * index<2> и index<3> используются в двумерных и трёхмерных массивах соответственно
		 */
		[=](index<2> idx) restrict(amp) {
		sum[idx] = a[idx] * b[idx];
	}
	);

	wcout << "Original matrix: " << endl;

	// Print the results.
	for (int i = 0; i < 5; i++) {
		for (int j = 0; j < 5; j++) {
			if (j > 0) {
				wcout << ",";
			}
			wcout << a(i, j);
		}
		wcout << endl;
	}

	wcout << endl << "Multipled matrix: " << endl;

	// Print the results.
	for (int i = 0; i < 5; i++) {
		for (int j = 0; j < 5; j++) {
			if (j > 0) {
				wcout << ",";
			}
			wcout << sum(i, j);
		}
		wcout << endl;
	}
}

void AmpMatrixTranspose() {
	int aCPP[] = {
		1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12,
		10, 20, 30, 40, 50, 60, 70, 80, 90, 100, 110, 120 };
	int sumCPP[R*C];

	// AMP объекты для обмена информации
	array_view<int, 2> a(R, C, aCPP);				// Это нужно для чтения в потоках и представления массива, как одномерный
	array_view<int, 2> sum(C, R, sumCPP);			// А тут для записи результата переменную функцию

	sum.discard_data();								// Оптимизация, типа очижения переменной

	parallel_for_each(
		sum.extent,										// Массив, хранящий в себе количество строк и колон: [строк, колон]
		[=](index<2> idx) restrict(amp) {
			sum[idx] = a(idx[1], idx[0]);
		}
	);

	wcout << "Original matrix: " << endl;

	// Print the results.
	for (int i = 0; i < R; i++) {
		for (int j = 0; j < C; j++) {
			if (j > 0) {
				wcout << ",";
			}
			wcout << a(i, j);
		}
		wcout << endl;
	}

	wcout << endl << "Transposed matrix: " << endl;

	for (int i = 0; i < C; i++) {
		for (int j = 0; j < R; j++) {
			if (j > 0) {
				wcout << ",";
			}
			wcout << sum(i, j);
		}
		wcout << endl;
	}
}

void Standart() {
	int arr_a[] = { 1, 2, 3, 4, 5 };
	int arr_b[] = { 6, 7, 8, 9, 10 };
	int arr_sum[t_size];

	for (int i = 0; i < 5; i++)
	{
		arr_sum[i] = arr_a[i] + arr_b[i];
	}

	for (int i = 0; i < 5; i++)
	{
		wcout << arr_sum[i] << endl;
	}


}

int main() {
	wcout << " 1) Accelerators list: \n";
	auto accelerators = accelerator::get_all();
	for (const auto& accel : accelerators) {
		wcout << accel.get_description() << endl;
	}
	wcout << endl;

	wcout << " 2) Vector sum:" << endl;
	//wcout << "Standart method" << endl;
	//Standart();
	CppAmpMethod();

	wcout << endl << endl << " 3) Matrix multiplication by number " << N << " :" << endl;
	//Matrix();
	AmpMatrixMultiplication();

	wcout << endl << " 4) Matrix transpose:" << endl;
	AmpMatrixTranspose();

	wcout << endl << " 5) Matrix multiplication by matrix:" << endl;
	AmpMatrixMultipledByMatrix();

	return 0;
}