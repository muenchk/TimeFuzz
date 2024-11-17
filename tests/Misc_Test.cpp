#include <iostream>
#include <memory>
#include <fstream>
#include <filesystem>
#include <exception>
#include <string>

#include "Grammar.h"
#include "DerivationTree.h"
#include "Data.h"


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


	Data* data = new Data();

	std::shared_ptr<Grammar> grammar = data->CreateForm<Grammar>();
	grammar->ParseScala("../../FormatExamples/grammar3.scala");
	std::string scala = grammar->Scala(true);
	logmessage("Full Grammar:\n{}", scala);

	auto stree = data->CreateForm<DerivationTree>();
	grammar->Derive(stree, 10, 0);
	auto input = data->CreateForm<Input>();
	input->derive = stree;

	auto generator = data->CreateForm<Generator>();
	generator->Generate(input, grammar, data->CreateForm<SessionData>());

	auto dtree = data->CreateForm<DerivationTree>();
	grammar->Extract(stree, dtree, 2, 3, 10, false);
	auto dctree = data->CreateForm<DerivationTree>();
	grammar->Extract(stree, dctree, 2, 3, 10, true);
}
