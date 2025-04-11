
#include "Logging.h"
#if defined(unix) || defined(__unix__) || defined(__unix)
#	include <cstdlib>
#	include <unistd.h>
#	include <sys/wait.h>
#endif
#if defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
#	include "ChrashHandlerINCL.h"
#endif

#include <chrono>

#include "Allocators.h"

void InitNodeCA(DerivationTree::NonTerminalNode* node, Allocator<DerivationTree::NonTerminalNode>* alloc, int depth)
{
	if (depth < 4)
	{
		depth++;
		for (int i = 0; i < 5; i++)
		{
			auto child = alloc->New();
			node->AddChild(child);
			InitNodeCA(child, alloc, depth);
		}
	}
} 

void InitNode(DerivationTree::NonTerminalNode* node, int depth)
{
	if (depth < 4) {
		depth++;
		for (int i = 0; i < 5; i++) {
			auto child = new DerivationTree::NonTerminalNode();
			node->AddChild(child);
			InitNode(child, depth);
		}
	}
}

void TesterCA(int num)
{
	StartProfiling;
	Allocators::InitThreadAllocators(std::this_thread::get_id(), (size_t)10000, true);
	auto alloc = Allocators::GetThreadAllocators(std::this_thread::get_id());
	auto nontermalloc = alloc->DerivationTree_NonTerminalNode();
	auto termalloc = alloc->DerivationTree_TerminalNode();
	auto seqalloc = alloc->DerivationTree_SequenceNode();
	std::vector<DerivationTree::Node*> _nodesNT;
	std::vector<DerivationTree::Node*> _nodesT;
	std::vector<DerivationTree::Node*> _nodesS;
	_nodesNT.resize(1000);
	_nodesT.resize(1000);
	_nodesS.resize(1000);
	for (int32_t i = 0; i < 100; i++) {
		for (int32_t c = 0; c < 100; c++)
		{
			_nodesNT[c] = nontermalloc->New();
			InitNodeCA((DerivationTree::NonTerminalNode*)_nodesNT[c], nontermalloc, 0);
			_nodesT[c] = termalloc->New();
			_nodesS[c] = seqalloc->New();
			InitNodeCA((DerivationTree::NonTerminalNode*)_nodesS[c], nontermalloc, 0);
		}
		for (size_t c = 0; c < 100; c++)
		{
			nontermalloc->Delete((DerivationTree::NonTerminalNode*)_nodesNT[c]);
			termalloc->Delete((DerivationTree::TerminalNode*)_nodesT[c]);
			seqalloc->Delete((DerivationTree::SequenceNode*)_nodesS[c]);
			_nodesNT[c] = nullptr;
			_nodesT[c] = nullptr;
			_nodesS[c] = nullptr;
		}
	}
	std::chrono::nanoseconds ns = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - $$profiletimebegin$$);
	std::cout << num << ": Time taken with Custom Allocator: " << Logging::FormatTime(std::chrono::duration_cast<std::chrono::microseconds>(ns).count()) << "\n";
	profile(TimeProfiling, "{}: Time taken with Custom Allocator, Allocs: {}|{}|{}, Reallocs: {}|{}|{}", num, nontermalloc->_unalloc, termalloc->_unalloc, seqalloc->_unalloc, nontermalloc->_realloc, termalloc->_realloc, seqalloc->_realloc);
	Allocators::DestroyThreadAllocators(std::this_thread::get_id());
}

void Tester(int num)
{
	StartProfiling;
	std::vector<DerivationTree::Node*> _nodesNT;
	std::vector<DerivationTree::Node*> _nodesT;
	std::vector<DerivationTree::Node*> _nodesS;
	_nodesNT.resize(1000);
	_nodesT.resize(1000);
	_nodesS.resize(1000);
	for (int32_t i = 0; i < 100; i++) {
		for (int32_t c = 0; c < 100; c++) {
			_nodesNT[c] = new DerivationTree::NonTerminalNode();
			InitNode((DerivationTree::NonTerminalNode*)_nodesNT[c], 0);
			_nodesT[c] = new DerivationTree::TerminalNode();
			InitNode((DerivationTree::NonTerminalNode*)_nodesNT[c], 0);
			_nodesS[c] = new DerivationTree::SequenceNode();
		}
		for (size_t c = 0; c < 100; c++) {
			delete _nodesNT[c];
			delete _nodesT[c];
			delete _nodesS[c];
			_nodesNT[c] = nullptr;
			_nodesT[c] = nullptr;
			_nodesS[c] = nullptr;
		}
	}
	std::chrono::nanoseconds ns = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - $$profiletimebegin$$);
	std::cout << num << ": Time taken with default Allocator: " << Logging::FormatTime(std::chrono::duration_cast<std::chrono::microseconds>(ns).count()) << "\n";
	profile(TimeProfiling, "{}: Time taken with default Allocator", num);
}

int main(int argc, char** argv)
{
	Logging::InitializeLog(".");
#if defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
	Crash::Install(".");
#endif

	auto allocators = Allocators::GetThreadAllocators(std::this_thread::get_id());
	
	int num = 10;
	if (argc > 1)
	{
		try {
			num = std::stoi(argv[1]);
		}
		catch (std::exception&) {
			num = 10;
		}
	}
	std::vector<std::thread> threads;
	threads.resize(num);

	StartProfiling;
	std::thread th0 (TesterCA, 0);
	th0.join();
	profile(TimeProfiling, "Time Taken for a single thread with Custom Allocator");
	ResetProfiling;
	std::thread th01(Tester, 0);
	th01.join();
	profile(TimeProfiling, "Time Taken for a single thread with default Allocator");

	
	ResetProfiling;
	for (int i = 0; i < num; i++) {
		threads[i] = std::thread(TesterCA, i);
	}
	for (int i = 0; i < num; i++) {
		threads[i].join();
	}
	/*std::thread th1(TesterCA, 1);
	std::thread th2(TesterCA, 2);
	std::thread th3(TesterCA, 3);
	std::thread th4(TesterCA, 4);
	std::thread th5(TesterCA, 5);
	std::thread th6(TesterCA, 6);
	std::thread th7(TesterCA, 7);
	std::thread th8(TesterCA, 8);
	std::thread th9(TesterCA, 9);
	std::thread th10(TesterCA, 10);
	std::thread th11(TesterCA, 11);
	std::thread th12(TesterCA, 12);
	std::thread th13(TesterCA, 13);
	std::thread th14(TesterCA, 14);
	std::thread th15(TesterCA, 15);
	std::thread th16(TesterCA, 16);
	std::thread th17(TesterCA, 17);
	std::thread th18(TesterCA, 18);
	std::thread th19(TesterCA, 19);
	std::thread th20(TesterCA, 20);
	th1.join();
	th2.join();
	th3.join();
	th4.join();
	th5.join();
	th6.join();
	th7.join();
	th8.join();
	th9.join();
	th10.join();
	th11.join();
	th12.join();
	th13.join();
	th14.join();
	th15.join();
	th16.join();
	th17.join();
	th18.join();
	th19.join();
	th20.join();*/
	profile(TimeProfiling, "Time Taken for 20 threads with Custom Allocator");
	ResetProfiling;
	for (int i = 0; i < num; i++) {
		threads[i] = std::thread(Tester, i);
	}
	for (int i = 0; i < num; i++) {
		threads[i].join();
	}
	/* std::thread th21(Tester, 21);
	std::thread th22(Tester, 22);
	std::thread th23(Tester, 23);
	std::thread th24(Tester, 24);
	std::thread th25(Tester, 25);
	std::thread th26(Tester, 26);
	std::thread th27(Tester, 27);
	std::thread th28(Tester, 28);
	std::thread th29(Tester, 29);
	std::thread th30(Tester, 30);
	std::thread th31(Tester, 31);
	std::thread th32(Tester, 32);
	std::thread th33(Tester, 33);
	std::thread th34(Tester, 34);
	std::thread th35(Tester, 35);
	std::thread th36(Tester, 36);
	std::thread th37(Tester, 37);
	std::thread th38(Tester, 38);
	std::thread th39(Tester, 39);
	std::thread th40(Tester, 40);
	th21.join();
	th22.join();
	th23.join();
	th24.join();
	th25.join();
	th26.join();
	th27.join();
	th28.join();
	th29.join();
	th30.join();
	th31.join();
	th32.join();
	th33.join();
	th34.join();
	th35.join();
	th36.join();
	th37.join();
	th38.join();
	th39.join();
	th40.join();*/
	profile(TimeProfiling, "Time Taken for 20 threads with default Allocator");

}
