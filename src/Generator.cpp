#include "Generator.h"
#include "BufferOperations.h"
#include "Logging.h"
#include "Grammar.h"
#include "Data.h"
#include "DerivationTree.h"
#include "SessionFunctions.h"

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
	static size_t size0x1 = 4                          // version
	                        + 8;                       // Grammar Form ID
	switch (version)
	{
	case 0x1:
		return size0x1;
	default:
		return 0;
	}
}

size_t Generator::GetDynamicSize()
{
	return Form::GetDynamicSize()  // form stuff
	       + GetStaticSize(classversion);
}

bool Generator::WriteData(std::ostream* buffer, size_t& offset)
{
	Buffer::Write(classversion, buffer, offset);
	Form::WriteData(buffer, offset);
	if (_grammar)
		Buffer::Write(_grammar->GetFormID(), buffer, offset);
	else
		Buffer::Write((uint64_t)0, buffer, offset);
	return true;
}

bool Generator::ReadData(std::istream* buffer, size_t& offset, size_t length, LoadResolver* resolver)
{
	int32_t version = Buffer::ReadInt32(buffer, offset);
	switch (version) {
	case 0x1:
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

void Generator::GenInputFromDevTree(std::shared_ptr<Input> input)
{
	if (input->derive->_root->Type() == DerivationTree::NodeType::Terminal) {
		input->AddEntry(((DerivationTree::TerminalNode*)(input->derive->_root))->_content);
	} else {
		std::vector<DerivationTree::SequenceNode*> seqnodes;

		std::stack<DerivationTree::NonTerminalNode*> stack;
		stack.push((DerivationTree::NonTerminalNode*)(input->derive->_root));
		DerivationTree::NonTerminalNode* tmp = nullptr;
		DerivationTree::Node* ntmp = nullptr;

		while (stack.size() > 0) {
			tmp = stack.top();
			stack.pop();
			if (tmp->Type() == DerivationTree::NodeType::Sequence)
				seqnodes.push_back((DerivationTree::SequenceNode*)tmp);
			// push children in reverse order to stack: We explore from left to right
			// skip terminal children, they cannot produce sequences
			for (int32_t i = (int32_t)tmp->_children.size() - 1; i >= 0; i--) {
				if (tmp->_children[i]->Type() != DerivationTree::NodeType::Terminal)
					stack.push((DerivationTree::NonTerminalNode*)(tmp->_children[i]));
			}
		}
		// we have found all sequence nodes
		// now traverse each one from left to right with depth first search and patch together
		// the individual _input sequences
		std::stack<DerivationTree::Node*> nstack;
		for (int32_t c = 0; c < (int32_t)seqnodes.size(); c++) {
			std::string entry = "";
			// push root node to stack
			nstack.push(seqnodes[c]);
			// gather _input fragments
			while (nstack.size() > 0) {
				ntmp = nstack.top();
				nstack.pop();
				// if the current child is terminal, it has to be left-most child in the remaining tree
				// and gets added to the entry
				if (ntmp->Type() == DerivationTree::NodeType::Terminal)
					entry += ((DerivationTree::TerminalNode*)ntmp)->_content;
				else {
					tmp = (DerivationTree::NonTerminalNode*)ntmp;
					// push children in reverse order: we traverse from left to right
					for (int32_t i = (int32_t)tmp->_children.size() - 1; i >= 0; i--) {
						nstack.push((tmp->_children[i]));
					}
				}
			}
			input->AddEntry(entry);
			if (input->GetTrimmedLength() != -1 && (int64_t)input->Length() == input->GetTrimmedLength())
				break;
		}
	}
}

bool Generator::Generate(std::shared_ptr<Input>& input, std::shared_ptr<Grammar> grammar, std::shared_ptr<SessionData> sessiondata, std::shared_ptr<Input> parent)
{
	StartProfiling;

	auto checkInheritance = [](Data* data, std::shared_ptr<Input> input) {
		while (input) {
			auto derive = input->derive;
			logmessage("{}\t\t{}", Input::PrintForm(input), DerivationTree::PrintForm(derive));
			input = data->LookupFormID<Input>(input->GetParentID());
		}
	};

	FlagHolder inputflag(input, Form::FormFlags::DoNotFree);
	std::unique_ptr<FlagHolder<Input>> parentflag;
	std::unique_ptr<FlagHolder<DerivationTree>> parenttreeflag;
	std::shared_ptr<Grammar> gram = _grammar;
	if (grammar)
		gram = grammar;
	if (gram) {
		input->Lock();
		if (!input->HasFlag(Input::Flags::GeneratedDeltaDebugging)) {
			// we only need to generate a new tree if there isn't a valid one
			// this can happen with temporary _input forms, that reuse existing
			// derivation trees, even though the _input forms themselves don't have
			// the derived sequences
			input->derive->Lock();
			if (input->derive->_valid == false) {
				if (input->HasFlag(Input::Flags::GeneratedGrammarParent)) {
					// implement derivation from a parent derivationtree here
					if (input->derive && input->derive->_regenerate == true)
					{
						// the only difference between re-extending and extending in the first place is that the parent information either
						// comes from the caller or in the case of re-generating from the input itself
						auto parenttree = sessiondata->data->LookupFormID<DerivationTree>(input->derive->_parent._parentID);
						if (parenttree)
							parent = sessiondata->data->LookupFormID<Input>(parenttree->_inputID);
					}
					// check parent
					if (!parent) {
						input->derive->Unlock();
						input->Unlock();
						return false;
					} else {
						parentflag = std::make_unique<FlagHolder<Input>>(parent, Form::FormFlags::DoNotFree);
						parenttreeflag = std::make_unique<FlagHolder<DerivationTree>>(parent->derive, Form::FormFlags::DoNotFree);
					}
					// we have a valid parent so make sure to generate it if it isn't already so we can generate this input
					if (parent->GetGenerated() == false) {
						// we are trying to add an _input that hasn't been generated or regenerated
						// try the generate it and if it succeeds add the test
						SessionFunctions::GenerateInput(parent, sessiondata);
						if (parent->GetGenerated() == false) {
							logwarn("Cannot generate input as parent input cannot be regenerated, Parent: {}", Input::PrintForm(parent));
							checkInheritance(sessiondata->data, parent);
							SessionFunctions::GenerateInput(parent, sessiondata);
							profile(TimeProfiling, "Time taken for input Generation");
							input->derive->Unlock();
							input->Unlock();
							return false;
						}
					}
					if (parent->derive->_valid == false) {
						if (parent->derive->_regenerate) {
							// we are trying to add an _input that hasn't been generated or regenerated
							// try the generate it and if it succeeds add the test
							SessionFunctions::GenerateInput(parent, sessiondata);
							if (parent->GetGenerated() == false || parent->derive->_valid == false) {
								logwarn("Cannot generate input as parent input cannot be regenerated, Parent: {}", Input::PrintForm(parent));
								SessionFunctions::GenerateInput(parent, sessiondata);
								profile(TimeProfiling, "Time taken for input Generation");
								input->derive->Unlock();
								input->Unlock();
								return false;
							}
						} else {
							logwarn("Cannot generate input as parent input cannot be regenerated, Parent: {}", Input::PrintForm(parent));
							profile(TimeProfiling, "Time taken for input Generation");
							input->derive->Unlock();
							input->Unlock();
							return false;
						}
					}

					if (input->derive && input->derive->_regenerate == true) {
						// now extend input
						if (input->HasFlag(Input::Flags::GeneratedGrammarParentBacktrack))
							gram->Extend(parent, input->derive, true, input->derive->_targetlen, input->derive->_seed);
						else
							gram->Extend(parent, input->derive, false, input->derive->_targetlen, input->derive->_seed);
					}
					else {
						// this is a new input
						std::uniform_int_distribution<signed> dist(sessiondata->_settings->generation.generationLengthMin, sessiondata->_settings->generation.generationLengthMax);
						int32_t sequencelen = dist(randan);
						// now extend input
						if (input->HasFlag(Input::Flags::GeneratedGrammarParentBacktrack))
							gram->Extend(parent, input->derive, true, sequencelen, (unsigned int)(std::chrono::system_clock::now().time_since_epoch().count()));
						else
							gram->Extend(parent, input->derive, false, sequencelen, (unsigned int)(std::chrono::system_clock::now().time_since_epoch().count()));
						// set parent information and flags
						input->SetParentGenerationInformation(parent->GetFormID());
						// increase number of inputs derived from parent
						parent->IncDerivedInputs();
					}
					// free the parents DerivationTree

				} else {
					if (input->derive && input->derive->_regenerate == true) {
						// we already generated the _input some time ago, so we will reuse the past generation parameters to
						// regenerate the same derivation tree
						int32_t sequencelen = input->derive->_targetlen;
						uint32_t seed = input->derive->_seed;
						gram->Derive(input->derive, sequencelen, seed);
					} else {
						std::uniform_int_distribution<signed> dist(sessiondata->_settings->generation.generationLengthMin, sessiondata->_settings->generation.generationLengthMax);
						int32_t sequencelen = dist(randan);
						// derive a derivation tree from the grammar
						gram->Derive(input->derive, sequencelen, (unsigned int)(std::chrono::system_clock::now().time_since_epoch().count()));
						input->SetFlag(Input::Flags::GeneratedGrammar);
						// gather the complete _input sequence
					}
				}
			}
			// get the _input sequence from the derivationtree
			if (input->derive->_valid && input->GetSequenceLength() == 0) {
				GenInputFromDevTree(input);
				input->SetGenerated();
				profile(TimeProfiling, "Time taken for input Generation");
				if ((int64_t)input->Length() != input->derive->_sequenceNodes && (!input->IsTrimmed() || (int64_t)input->Length() != input->GetTrimmedLength()))
					logwarn("The input length is different from the generated sequence. Length: {}, Expected: {}, Trimmed: {}", input->Length(), input->derive->_sequenceNodes, input->GetTrimmedLength());
				if ((int64_t)input->Length() == 0)
					logwarn("The input length is 0.");

				input->derive->Unlock();
				input->Unlock();
				return true;
			} else if (input->derive->_valid && input->GetSequenceLength() > 0)
				input->SetGenerated();
			input->derive->Unlock();
		} else {
			// the input has beeen created by delta debuggging, so reconstruct it
			// first get parent input
			parent = sessiondata->data->LookupFormID<Input>(input->GetParentID());
			if (!parent) {
				profile(TimeProfiling, "Time taken for input Generation");
				input->Unlock();
				return false;
			} else {
				parentflag = std::make_unique<FlagHolder<Input>>(parent, Form::FormFlags::DoNotFree);
			}
			// we have a valid parent so make sure to generate it if it isn't already so we can generate this input
			if (parent->GetGenerated() == false) {
				// we are trying to add an _input that hasn't been generated or regenerated
				// try the generate it and if it succeeds add the test
				SessionFunctions::GenerateInput(parent, sessiondata);
				if (parent->GetGenerated() == false) {
					logwarn("Cannot generate input as parent input cannot be regenerated, Parent: {}", Input::PrintForm(parent))
						profile(TimeProfiling, "Time taken for input Generation");
					input->Unlock();
					return false;
				}
			}
			if (parent->derive->_valid == false) {
				if (parent->derive->_regenerate) {
					// we are trying to add an _input that hasn't been generated or regenerated
					// try the generate it and if it succeeds add the test
					SessionFunctions::GenerateInput(parent, sessiondata);
					if (parent->GetGenerated() == false || parent->derive->_valid == false) {
						logwarn("Cannot generate input as parent input cannot be regenerated, Parent: {}", Input::PrintForm(parent));
						profile(TimeProfiling, "Time taken for input Generation");
						input->derive->Unlock();
						input->Unlock();
						return false;
					}
				} else {
					logwarn("Cannot generate input as parent input cannot be regenerated, Parent: {}", Input::PrintForm(parent));
					profile(TimeProfiling, "Time taken for input Generation");
					input->derive->Unlock();
					input->Unlock();
					return false;
				}
			}
			input->derive->Lock();

			// extract the corresponding derivation tree
			if (input->derive->_valid == false) {
				auto segments = input->GetParentSplits();
				gram->Extract(parent->derive, input->derive, segments, parent->Length(), input->GetParentSplitComplement());
				if (input->derive->_valid == false)
				{
					// the input cannot be derived from the given grammar
					logwarn("The input {} cannot be derived from the grammar.", Utility::GetHex(input->GetFormID()));
					input->Unlock();
					return false;
				}
			}
			input->derive->Unlock();

			if (input->derive->_valid && input->GetSequenceLength() == 0) {
				// getting this from the devtree is slower but is more more code change resistant
				GenInputFromDevTree(input);
				input->SetGenerated();
				profile(TimeProfiling, "Time taken for input Generation");
				if ((int64_t)input->Length() != input->derive->_sequenceNodes && (!input->IsTrimmed() || (int64_t)input->Length() != input->GetTrimmedLength())) {
					logwarn("The input length is different from the generated sequence. Length: {}, Expected: {}", input->Length(), input->derive->_sequenceNodes);
				}
				if ((int32_t)input->Length() == 0)
					logwarn("The input length is 0.");

				input->SetGenerated();
				input->Unlock();
				return true;

			} else if (input->derive->_valid)
				input->SetGenerated();
		}
		profile(TimeProfiling, "Time taken for input Generation");
		input->Unlock();
		return false;
	} else {
		DummyGenerate(input);
		input->SetGenerated();
	}
	profile(TimeProfiling, "Time taken for input Generation");
	if ((int64_t)input->Length() != input->derive->_sequenceNodes && (!input->IsTrimmed() || (int64_t)input->Length() != input->GetTrimmedLength())) {
		logwarn("The length of {} is different from the generated sequence. Length: {}, Expected: {}", Input::PrintForm(input), input->Length(), input->derive->_sequenceNodes);
	}
	return true;
}


void SimpleGenerator::Clean()
{

}
