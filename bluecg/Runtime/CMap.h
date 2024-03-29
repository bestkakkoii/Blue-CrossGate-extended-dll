#pragma once
#ifndef CMAP_H
#define CMAP_H
#include <Windows.h>

#include <vector>

class CMap
{
	enum class ObjectType
	{
		UP,
		DOWN,
		JUMP,
		WALL,
		ROCK,
		ROAD,
		EMPTY,
		UNKNOWN,
	};
public:
	bool WINAPI AddNewMap();
	int WINAPI isCodValid(const int& x, const int& y)
	{
		if (v.size() == 0)
			return 0;
		if (x > g_w || y > g_h || x <= 0 || y <= 0)
			return 0;
		if (v.size() <= 0)
			return -1;
		if (v[x][y])
			return 1;
		return 0;
	}

private:

	std::vector<std::vector<int>> v;
	int g_w = 0;
	int g_h = 0;
	const int WINAPI GetObjectType(const int& gnum);
};

#endif