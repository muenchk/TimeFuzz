#include <iostream>
#include <memory>
#include <fstream>
#include <filesystem>
#include <exception>
#include <string>


/// <summary>
/// unique name of the save [i.e. "Testing"]
/// </summary>
std::string uniquename;
/// <summary>
/// save name of the save itself, contains unique name and saveidentifiers
/// </summary>
std::string savename;
/// <summary>
/// file extension used by savefiles
/// </summary>
std::string extension = ".tfsave";
/// <summary>
/// path to the save files
/// </summary>
std::filesystem::path savepath = std::filesystem::path(".") / "saves";
/// <summary>
/// number of the next save
/// </summary>
int32_t savenumber = 1;

std::string GetSaveName()
{
	std::string name = uniquename + "_" + std::to_string(savenumber) + extension;
	savenumber++;
	return name;
}

int main()
{
	std::string name = GetSaveName();
	if (!std::filesystem::exists(savepath))
		std::filesystem::create_directories(savepath);
	std::ofstream fsave = std::ofstream((savepath / name), std::ios_base::out | std::ios_base::binary);
	printf("%s\n", (savepath / name).string().c_str());
}
