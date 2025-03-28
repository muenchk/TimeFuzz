#include <iostream>
#include <memory>
#include <fstream>
#include <filesystem>
#include <exception>
#include <string>

#include "Grammar.h"
#include "DerivationTree.h"
#include "Data.h"
#include "SessionFunctions.h"
#include "Input.h"
#include "DerivationTree.h"
#include "Test.h"


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
	auto sessdata = data->CreateForm<SessionData>();
	generator->Generate(input, {}, grammar, sessdata);

	auto dtree = data->CreateForm<DerivationTree>();
	std::vector<std::pair<int64_t, int64_t>> segments1 = { { 2, 3 }, { 7, 2 } };
	std::vector<std::pair<int64_t, int64_t>> segments2 = { { 2, 3 } };
	grammar->Extract(stree, dtree, segments1, 10, false);
	auto dctree = data->CreateForm<DerivationTree>();
	grammar->Extract(stree, dctree, segments2, 10, true);

	
	auto inp = data->CreateForm<Input>();
	inp->derive = dtree;
	inp->SetParentSplitInformation(input->GetFormID(), segments1, false);
	generator->Generate(inp, {}, grammar, sessdata);

	auto exttree = data->CreateForm<DerivationTree>();
	auto extinput = data->CreateForm<Input>();
	extinput->derive = exttree;
	input->SetFlag(Input::Flags::GeneratedGrammarParent);
	int32_t back = 0;
	grammar->Extend(inp, exttree, false, 15, 0,back);

	generator->Generate(extinput, {}, grammar, sessdata);
}
