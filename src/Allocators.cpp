#include "Allocators.h"
#include "Threading.h"

void Allocators::InitThreadAllocators(std::thread::id id)
{
	SpinlockA guard(__threadmapFlag);
	auto alloc = new Allocators();
	__threadmap.insert_or_assign(id, alloc);
}

void Allocators::InitThreadAllocators(std::thread::id id, size_t minsize)
{
	auto alloc = new Allocators();
	{
		SpinlockA guard(__threadmapFlag);
		__threadmap.insert_or_assign(id, alloc);
	}
	
	alloc->DerivationTree_NonTerminalNode()->Alloc(minsize);
	alloc->DerivationTree_SequenceNode()->Alloc(minsize);
	alloc->DerivationTree_TerminalNode()->Alloc(minsize);
}

void Allocators::DestroyThreadAllocators(std::thread::id id)
{
	Allocators* alloc = nullptr;
	{
		SpinlockA guard(__threadmapFlag);
		try {
			alloc = __threadmap.at(id);
		} catch (std::exception&) {
			return;
		}
	}
	if (alloc)
		delete alloc;
}

Allocators* Allocators::GetThreadAllocators(std::thread::id id)
{
	try {
		return __threadmap.at(id);
	} catch (std::exception&) {
		SpinlockA guard(__threadmapFlag);
		auto all = new Allocators();
		__threadmap.insert_or_assign(id, all);
		return all;
	}
}

Allocator<DerivationTree::NonTerminalNode>* Allocators::DerivationTree_NonTerminalNode()
{
	return &_dev_nonterm;
}
Allocator<DerivationTree::TerminalNode>* Allocators::DerivationTree_TerminalNode()
{
	return &_dev_term;
}
Allocator<DerivationTree::SequenceNode>* Allocators::DerivationTree_SequenceNode()
{
	return &_dev_seq;
}
