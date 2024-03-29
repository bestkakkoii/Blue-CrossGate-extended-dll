#include "CMap.h"

#include <iostream>
#include <filesystem>

#include <dbghelp.h>

//#include <format>

//#pragma comment(lib, "Dbghelp.lib")

const int CMap::GetObjectType(const int& gnum)
{
	static bool exist;
	const int UP[] = { 12000, 12001, 13268, 13270, 13272, 13274, 13996, 13998, 15561, 15887, 15889, 15891, 17952,
						 17954, 17956, 17958, 17960, 17962, 17964, 17966, 17968, 17970, 17972, 17974, 17976, 17978,
						 17980, 17982, 17984, 17986, 17988, 17990, 17992, 17994, 17996, 17998, 16610, 16611, 16626,
						 16627, 16628, 16629 };
	exist = std::find(std::begin(UP), std::end(UP), gnum) != std::end(UP);
	if (exist)
	{
		return (int)ObjectType::UP;
	}
	const int DOWN[] = { 12002, 12003, 13269, 13271, 13273, 13275, 13997, 13999, 15562, 15888, 15890, 15892, 17953,
						   17955, 17957, 17959, 17961, 17963, 17965, 17967, 17969, 17971, 17973, 17975, 17977, 17979,
						   17981, 17983, 17985, 17987, 17989, 17991, 17993, 17995, 17997, 17999, 16612, 16613, 16614,
						   16615 };
	exist = std::find(std::begin(DOWN), std::end(DOWN), gnum) != std::end(DOWN);
	if (exist)
	{
		return (int)ObjectType::UP;
	}

	const int JUMP[] = { 0, 14676 };
	exist = std::find(std::begin(JUMP), std::end(JUMP), gnum) != std::end(JUMP);
	if (exist)
	{
		return (int)ObjectType::JUMP;
	}
	const int WALL[] = { 13461, 13465, 13440, 13453, 13446, 15312, 15313, 13432, 13464, 13458, 15314, 13444, 13459,
						   15310, 13439, 13451, 15309, 13462, 13436, 13447, 13438, 13467, 15307, 13463, 13466,
						   13442, 13443, 13450, 13437, 13460, 13454, 13455, 13449, 13457, 13433, 15306, 13445, 13456,
						   13434, 13441, 13448, 13435, 13452, 13613, 13636, 13633, 13637, 13632, 13623, 13647, 13631,
						   13643, 13635, 13618, 13619, 13624, 13640, 13634, 13617, 13644, 13642, 13620, 13637, 13630,
						   13616, 13622, 13633, 13614, 13625, 13627, 13639, 13626, 13638, 13628, 13615, 13646, 13645,
						   13629, 13641, 13621, 17700, 17701, 17702, 17703, 17704,  17705,17706,  17707,  17708, 17709,
						   17710,
						   17711,
						   17712,
						   17713,
						   17714,
						   17715,
						   17716,
						   17717,
						   17718,
						   17719,
						   17720,
						   17721,
						   17722,
						   17723,
						   17724,
						   17725,
						   17726,
						   17727,
						   17728,
						   17729,
						   17730,
						   17731,
						   17732,
						   17733,
						   17734,
						   17735,
						   17065,
						   13080,
						   13081,
						   13082,
						   13083,
						   13084,
						   13085,
						   13086,
						   13087,
						   13088,
						   13089,
						   13090,
						   13091,
						   13092,
						   13093,
						   13094,
						   13095,
						   13096,
						   13097,
						   13098,
						   13099,
						   13100,
						   13101,
						   13102,
						   13103,
						   13104,
						   13105,
						   13106,
						   13107,
						   13108,
						   13109,
						   13110,
						   13111,
						   13112,
						   13113,
						   13114,
						   13115,
						   12221,
		 12217,
		 12216,
		 12212,
		 15345,
		 15346,
		 15347,
		 15348,
		 15349,
		 15350,
		 15351,
		 15352,
		 15353,
		 15354,
		 15355,
		 15356,
		 15357,
		 15358,
		 15359,
		 15360,
		 15361,
		 15362,
		 15363,
		 15364,
		 15365,
		 15366,
		 15367,
		 15368,
		 15369,
		 15370,
		 15371,
		 15372,
		 15373,
		 15374,
		 15375,
		 15376,
		 15377,
		 15378,
		 15379,
		 15380,
		 13504,
		 13505,
		 13506,
		 13507,
		 13508,
		 13509,
		 13510,
		 13511,
		 13512,
		 13513,
		 13514,
		 13515,
		 13516,
		 13517,
		 13518,
		 13519,
		 13520,
		 13521,
		 13522,
		 13523,
		 13524,
		 13525,
		 13526,
		 13527,
		 13528,
		 13529,
		 13530,
		 13531,
		 13532,
		 13533,
		 13534,
		 13535,
		 13536,
		 13537,
		 13538,
		 13539,
		 13117,
		 13118,
		 13119,
		 13120,
		 13121,
		 13122,
		 13123,
		 13124,
		 13125,
		 13126,
		 13127,
		 13128,
		 13129,
		 13130,
		 13131,
		 13132,
		 13133,
		 13134,
		 13135,
		 13136,
		 13137,
		 13138,
		 13139 };
	exist = std::find(std::begin(WALL), std::end(WALL), gnum) != std::end(WALL);
	if (exist)
	{
		return (int)ObjectType::WALL;
	}
	const int EMPTY[] = { 5000, 5001, 5002, 5003, 5004, 5005, 5006, 5007, 5008, 5009 };
	exist = std::find(std::begin(EMPTY), std::end(EMPTY), gnum) != std::end(EMPTY);
	if (exist)
	{
		return (int)ObjectType::EMPTY;
	}
	const int ROCK[] = {
		12218, 12197, 12222, 12200, 12199, 12209, 12220, 12198, 12223, 15068,
		15070, 15076, 15080,15074, 15072, 15081, 15079,12202,12204,12205,
		12206,12207, 12208, 12209,12210,12211,33075,33076, 33077,33078, 33079,12357, 12479, 12484
	};

	exist = std::find(std::begin(ROCK), std::end(ROCK), gnum) != std::end(ROCK);
	if (exist)
	{
		return (int)ObjectType::ROCK;
	}

	const int ROAD[] = {
		12203,15304,15305,12497, 12499
	};

	exist = std::find(std::begin(ROAD), std::end(ROAD), gnum) != std::end(ROAD);
	if (exist)
	{
		return (int)ObjectType::ROAD;
	}

	return (int)ObjectType::UNKNOWN;
}

bool CMap::AddNewMap()
{
	//输入文件指针
	FILE* pFileIn = nullptr;
	//输入为.dat格式，path为文件名
	int mapid = (int)(*(DWORD*)(0x095C870 + 0x28));
	if (mapid == 0)
		return false;
	const std::wstring fileName = std::format(L"{}.dat", mapid);
	const std::wstring dir = std::filesystem::current_path();
	const std::wstring mappath = std::format(L"{}\\map", dir);
	WCHAR OutputPathBuffer[MAX_PATH * 2] = {};
	if (!SearchTreeForFileW(mappath.c_str(), fileName.c_str(), OutputPathBuffer))
		return false;

	pFileIn = _wfsopen(OutputPathBuffer, L"rb", SH_DENYNO);
	//若文件打开失败则提示错误
	if (pFileIn == nullptr)
		return false;
	int i = 0, j = 0, n = 0;
	bool bret = false;

	v.clear();

	g_w = 0; g_h = 0;

	//假设.dat中存储的数据为两个字节的无符号数，则数组tNum应为unsigned short（两个字节）
	unsigned short tNum[100];

	fseek(pFileIn, 12, SEEK_SET);
	fread(tNum, 1, 2, pFileIn);
	const int width = tNum[0];
	fseek(pFileIn, 16, SEEK_SET);
	fread(tNum, 1, 2, pFileIn);
	const int height = tNum[0];
	if ((width >= 1) && (height >= 1) && (width <= 1500) && (height <= 1500))
	{
		g_w = width;
		g_h = height;

		const int sectionOffset = width * height * 2;

		//int linelength = width * 3;
		//int bufferSize = linelength * height * 3 * width * 3;
		//BYTE* mapdata = new BYTE[bufferSize];
		std::vector<std::vector<int> > obj(width); //定义二维动态数组大小5行
		for (size_t m = 0; m < obj.size(); m++)//动态二维数组为5行6列，值全为0
		{
			obj[m].resize(height);
		}

		for (size_t q = 0; q < obj.size(); q++)//init二维动态数组
		{
			for (size_t k = 0; k < obj[q].size(); k++)
			{
				obj[q][k] = 0;
			}
		}

		for (i = 0; i < height; i++)
		{
			for (j = 0; j < width; j++)
			{
				//mapdata[i * linelength + j * 3] = 0;
				//mapdata[i * linelength + j * 3 + 1] = 0;
				//mapdata[i * linelength + j * 3 + 2] = 0;
				// 移到場景轉換數據塊 去掉檔頭20
				fseek(pFileIn, 20 + (j + i * width) * 2, SEEK_SET);
				fread(tNum, 1, 2, pFileIn);
				if (GetObjectType(tNum[0]) != (int)ObjectType::EMPTY)
				{
					if (tNum[0] > 0x64)
					{
						fseek(pFileIn, 20 + (j + i * width) * 2, SEEK_SET);//第一數據塊起點
						fseek(pFileIn, sectionOffset * 1, SEEK_CUR);//切換到第二數據塊
						fread(tNum, 1, 2, pFileIn);
						//排除牆壁,障礙,位置
						if (GetObjectType(tNum[0]) != (int)ObjectType::WALL && GetObjectType(tNum[0]) != (int)ObjectType::ROCK)
						{
							//mapdata[i * linelength + j * 3] = 0xff;
							//mapdata[i * linelength + j * 3 + 1] = 0xff;
							//mapdata[i * linelength + j * 3 + 2] = 0xff;
							// 回物件數據塊取迷宮可通行座標
							obj[i][j] = 1;
							bret = true;
						}
					}
				}
				n++;
			}
		}
		//delete[] mapdata;
		v = obj;
	}

	fclose(pFileIn);
	pFileIn = nullptr;
	return bret;
}