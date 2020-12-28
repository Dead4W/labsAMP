#pragma once
#include <windows.h>
#include "pch.h"
#include <iostream>
#include <omp.h>
#include "timer.h"
#include <time.h>

struct PerfomanceTest
{
	double Start(void(*func)(void))
	{
		return CalcElapsedTime(func);
	}

private:

	//Обычный sleep
	static void sleep(int milliseconds) // Cross-platform sleep function
	{
		clock_t time_end;
		time_end = clock() + milliseconds * CLOCKS_PER_SEC / 1000;
		while (clock() < time_end) {}
	}

	// Returns the overhead of the timer in ticks
	static double CalcElapsedTime(void(*func)(void))
	{
		double Elapsed = 0;
		int restarts = 10;

		// повторяем функцию несколько раз и берем среднее
		for (int i = 0;i < restarts; i++) {
			Timer timer = Timer(); // таймер
			timer.Start();
			func(); // сама функция
			timer.Stop();
			Elapsed += timer.Elapsed();
			sleep(150); //чтобы сбивать ускорение, иначе программа будет выполнятся с каждым разом быстрее
		}

		return Elapsed / restarts;
	}

};