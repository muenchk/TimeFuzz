#include "Generator.h"
#include "BufferOperations.h"
#include "Logging.h"
#include "Grammar.h"
#include "Data.h"
#include "DerivationTree.h"

#include <random>
#include <stack>

static std::mt19937 randan((unsigned int)(std::chrono::system_clock::now().time_since_epoch().count()));

void Generator::Clean()
{

}

void Generator::Clear()
{

}

void Generator::RegisterFactories()
{
	if (!_registeredFactories) {
		_registeredFactories = !_registeredFactories;
	}
}

size_t Generator::GetStaticSize(int32_t version)
{
	static size_t size0x1 = 4  // version
	                        + Form::GetDynamicSize(); // _formid
	static size_t size0x2 = 4                          // version
	                        + Form::GetDynamicSize()   // _formid
	                        + 8;                       // Grammar Form ID
	switch (version)
	{
	case 0x1:
		return size0x1;
	case 0x2:
		return size0x2;
	default:
		return 0;
	}
}

size_t Generator::GetDynamicSize()
{
	return GetStaticSize(classversion);
}

bool Generator::WriteData(unsigned char* buffer, size_t& offset)
{
	Buffer::Write(classversion, buffer, offset);
	Form::WriteData(buffer, offset);
	if (_grammar)
		Buffer::Write(_grammar->GetFormID(), buffer, offset);
	else
		Buffer::Write((uint64_t)0, buffer, offset);
	return true;
}

bool Generator::ReadData(unsigned char* buffer, size_t& offset, size_t length, LoadResolver* resolver)
{
	int32_t version = Buffer::ReadInt32(buffer, offset);
	switch (version) {
	case 0x1:
		Form::ReadData(buffer, offset, length, resolver);
		return true;
	case 0x2:
		{
			Form::ReadData(buffer, offset, length, resolver);
			FormID gram = Buffer::ReadUInt64(buffer, offset);
			resolver->AddTask([this, resolver, gram]() {
				this->_grammar = resolver->ResolveFormID<Grammar>(gram);
			});
			return true;
		}
	default:
		return false;
	}
}

void Generator::Delete(Data*)
{
	Clear();
}

void Generator::Init()
{

}

void Generator::SetGrammar(std::shared_ptr<Grammar> grammar)
{
	_grammar = grammar;
}

void DummyGenerate(std::shared_ptr<Input>& input)
{
	std::uniform_int_distribution<signed> dist(1000, 10000);
	int x = dist(randan);
	//static std::vector<std::string> buttons = { "[]", "[ \'RIGHT\' ]", "[ \'LEFT\' ]", "[ \'Z\' ]", "[ \'RIGHT\',  \'Z\'  ]", "[ \'LEFT\',  \'Z\'  ]" };
	static std::vector<std::string> buttons = { "[]", "[ \'RIGHT\' ]", "[ \'Z\' ]", "[ \'RIGHT\',  \'Z\'  ]" };
	static std::uniform_int_distribution<signed> butdist(0, (int)buttons.size() - 1);
	for (int i = 0; i < x; i++) {
		input->AddEntry(buttons[butdist(randan)]);
	}
}

bool Generator::Generate(std::shared_ptr<Input>& input, std::shared_ptr<Grammar> grammar)
{
	StartProfiling;
	std::shared_ptr<Grammar> gram = _grammar;
	if (grammar)
		gram = grammar;
	if (gram) {
		// we only need to generate a new tree if there isn't a valid one
		// this can happen with temporary input forms, that reuse existing
		// derivation trees, even though the input forms themselves don't have
		// the derived sequences
		input->derive->AcquireLock();
		if (input->derive->valid == false) {
			if (input->derive && input->derive->regenerate == true) {
				// we already generated the input some time ago, so we will reuse the past generation parameters to
				// regenerate the same derivation tree
				int32_t sequencelen = input->derive->targetlen;
				uint32_t seed = input->derive->seed;
				gram->Derive(input->derive, sequencelen, seed);
			} else {
				std::uniform_int_distribution<signed> dist(1000, 10000);
				int32_t sequencelen = dist(randan);
				// derive a derivation tree from the grammar
				gram->Derive(input->derive, sequencelen, (unsigned int)(std::chrono::system_clock::now().time_since_epoch().count()));
				// gather the complete input sequence
			}
		}
		// get the input sequence from the derivationtree
		if (input->derive->valid) {
			if (input->derive->root->Type() == DerivationTree::NodeType::Terminal) {
				input->AddEntry(((DerivationTree::TerminalNode*)(input->derive->root))->content);
			} else {
				std::vector<DerivationTree::SequenceNode*> seqnodes;

				std::stack<DerivationTree::NonTerminalNode*> stack;
				stack.push((DerivationTree::NonTerminalNode*)(input->derive->root));
				DerivationTree::NonTerminalNode* tmp = nullptr;
				DerivationTree::Node* ntmp = nullptr;

				while (stack.size() > 0) {
					tmp = stack.top();
					stack.pop();
					if (tmp->Type() == DerivationTree::NodeType::Sequence)
						seqnodes.push_back((DerivationTree::SequenceNode*)tmp);
					// push children in reverse order to stack: We explore from left to right
					// skip terminal children, they cannot produce sequences
					for (int32_t i = (int32_t)tmp->children.size() - 1; i >= 0; i--) {
						if (tmp->children[i]->Type() != DerivationTree::NodeType::Terminal)
							stack.push((DerivationTree::NonTerminalNode*)(tmp->children[i]));
					}
				}
				// we have found all sequence nodes
				// now traverse each one from left to right with depth first search and patch together
				// the individual input sequences
				std::stack<DerivationTree::Node*> nstack;
				for (int32_t c = 0; c < (int32_t)seqnodes.size(); c++) {
					std::string entry = "";
					// push root node to stack
					nstack.push(seqnodes[c]);
					// gather input fragments
					while (nstack.size() > 0) {
						ntmp = nstack.top();
						nstack.pop();
						// if the current child is terminal, it has to be left-most child in the remaining tree
						// and gets added to the entry
						if (ntmp->Type() == DerivationTree::NodeType::Terminal)
							entry += ((DerivationTree::TerminalNode*)ntmp)->content;
						else {
							tmp = (DerivationTree::NonTerminalNode*)ntmp;
							// push children in reverse order: we traverse from left to right
							for (int32_t i = (int32_t)tmp->children.size() - 1; i >= 0; i--) {
								nstack.push((tmp->children[i]));
							}
						}
					}
					input->AddEntry(entry);
					if (input->GetTrimmedLength() != -1 && (int64_t)input->Length() == input->GetTrimmedLength())
						break;
				}
			}
			input->SetGenerated();
			profile(TimeProfiling, "Time taken for input Generation");
			if ((int32_t)input->Length() != input->derive->sequenceNodes && input->GetTrimmedLength() != -1 && (int32_t)input->Length() != input->GetTrimmedLength())
				logwarn("The input length is different from the generated sequence. Length: {}, Expected: {}", input->Length(), input->derive->sequenceNodes);
			if ((int32_t)input->Length() == 0)
				logwarn("The input length is 0.");

			input->derive->ReleaseLock();
			return true;
		} else
			;
		profile(TimeProfiling, "Time taken for input Generation");
		input->derive->ReleaseLock();
		return false;
	} else {
		DummyGenerate(input);
		input->SetGenerated();
	}
	profile(TimeProfiling, "Time taken for input Generation");
	if ((int32_t)input->Length() != input->derive->sequenceNodes)
		logwarn("The input length is different from the generated sequence. Length: {}, Expected: {}", input->Length(), input->derive->sequenceNodes);
	return true;
}


void SimpleGenerator::Clean()
{

}
