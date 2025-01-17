#include "corpus.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <algorithm>

const double SIZE_RATIO = 5.0;

uint32_t getStringID(Corpus &corpus, std::string &str)
{
	auto iter = corpus.string2index.find(str);

	if (iter != corpus.string2index.end())
	{
		// already in map return index
		return iter->second;
	}
	else
	{
		// get the id of the new string
		uint32_t id = corpus.index2string.size();
		// add the string
		corpus.index2string.push_back(str);
		// map it to the new id
		corpus.string2index[str] = id;
		return id;
	}
}

Corpus load_corpus(const std::string &filename)
{
	Corpus corpus;
	std::ifstream file(filename);
	if (!file.is_open())
	{
		std::cerr << "Error: could not open file " << filename << std::endl;
		return corpus;
	}

	std::string line;
	// skip the first line of the file
	std::getline(file, line);
	int pos = 0;
	corpus.sentences.push_back(pos);

	while (std::getline(file, line))
	{
		// check if line is a comment
		if (line[0] == '#')
		{
			continue;
		}

		if (line.empty())
		{
			corpus.sentences.push_back(pos);
		}
		else
		{
			Token token;
			std::string word_str, c5_str, lemma_str, pos_str;
			std::istringstream iss(line);
			iss >> word_str >> c5_str >> lemma_str >> pos_str;
			// lookup variables
			token.word = getStringID(corpus, word_str);
			token.c5 = getStringID(corpus, c5_str);
			token.lemma = getStringID(corpus, lemma_str);
			token.pos = getStringID(corpus, pos_str);

			corpus.tokens.push_back(token);
			pos++;
		}
	}

	return corpus;
}

std::vector<Match> match(const Corpus &corpus, const std::string &query_string)
{
	return match(corpus, parse_query(query_string, corpus));
}

bool matchesLiteral(const Corpus &corpus, const Token &token, const Literal &literal)
{
	bool match = false;
	// if value is -1 we already know there are no match
	if (static_cast<int>(literal.value) == -1)
	{
		return false;
	}
	if (literal.attribute == "word")
	{
		match = (corpus.index2string[literal.value] == corpus.index2string[token.word]);
	}
	else if (literal.attribute == "c5")
	{
		match = (corpus.index2string[literal.value] == corpus.index2string[token.c5]);
	}
	else if (literal.attribute == "lemma")
	{
		match = (corpus.index2string[literal.value] == corpus.index2string[token.lemma]);
	}
	else if (literal.attribute == "pos")
	{
		match = (corpus.index2string[literal.value] == corpus.index2string[token.pos]);
	}
	else if (literal.attribute == "match all")
	{
		return true;
	}

	return literal.is_equality ? match : !match;
}

std::vector<Match> match(const Corpus &corpus, const Query &query)
{
	std::vector<Match> matches;
	// iterate over each sentence
	for (size_t i = 0; i < corpus.sentences.size(); i++)
	{
		size_t clause_matches = 0;
		// get scentence lenght
		int sentence_lenght = corpus.sentences[i + 1] - corpus.sentences[i];
		int start = corpus.sentences[i];
		int end = start + sentence_lenght;

		int pos = 0;

		for (int j = corpus.sentences[i]; j < end; j++)
		{
			bool match_found = true;
			int current_token = j;
			// iterate over
			while (clause_matches < query.size() && current_token < end)
			{
				const Clause &clause = query[clause_matches];
				bool clause_match = true;

				// check all literals in the clause
				for (const Literal &literal : clause)
				{
					if (!matchesLiteral(corpus, corpus.tokens[current_token], literal))
					{
						clause_match = false;
						break;
					}
				}

				// match found, move to next clause and token
				if (clause_match)
				{
					clause_matches++;
					current_token++;
				}
				else
				{
					match_found = false;
					break;
				}
			}

			// check if all matches where found
			if (match_found && clause_matches == query.size())
			{
				Match match;
				match.sentence = i;
				match.pos = pos;
				match.len = static_cast<int>(query.size());
				matches.push_back(match);
			}

			// reset for next token
			clause_matches = 0;
			pos++;
		}
	}

	return matches;
}
Index build_index(const std::vector<Token> &tokens, uint32_t Token::*attribute)
{
	Index index(tokens.size());

	for (size_t i = 0; i < tokens.size(); i++)
	{
		index[i] = i;
	}

	std::stable_sort(index.begin(), index.end(), [&](int a, int b)
					 { return tokens[a].*attribute < tokens[b].*attribute; });

	return index;
}

void build_indices(Corpus &corpus)
{
	corpus.c5_index = build_index(corpus.tokens, &Token::c5);
	corpus.lemma_index = build_index(corpus.tokens, &Token::lemma);
	corpus.word_index = build_index(corpus.tokens, &Token::word);
	corpus.pos_index = build_index(corpus.tokens, &Token::pos);
}

IndexSet index_lookup(const Corpus &corpus, const std::string &attribute, uint32_t value)
{
	const Index *index;
	uint32_t Token::*attribute_ptr;

	if (attribute == "lemma")
	{
		index = &corpus.lemma_index;
		attribute_ptr = &Token::lemma;
	}
	else if (attribute == "word")
	{
		index = &corpus.word_index;
		attribute_ptr = &Token::word;
	}
	else if (attribute == "c5")
	{
		index = &corpus.c5_index;
		attribute_ptr = &Token::c5;
	}
	else if (attribute == "pos")
	{
		index = &corpus.pos_index;
		attribute_ptr = &Token::pos;
	}
	else
	{
		// not possible to reach this since parser checks the atributes
		exit(1);
	}

	auto begin = index->begin();
	auto end = index->end();

	auto first = std::lower_bound(begin, end, value,
								  [&](int pos, uint32_t val)
								  {
									  return corpus.tokens[pos].*attribute_ptr < val;
								  });

	// find the position after the last occurrence of the value
	auto last = std::upper_bound(first, end, value,
								 [&](uint32_t val, int pos)
								 {
									 return val < corpus.tokens[pos].*attribute_ptr;
								 });

	size_t first_index = std::distance(begin, first);
	size_t last_index = std::distance(begin, last);

	IndexSet index_set;
	index_set.elems = std::span<const int>(index->begin() + first_index, last_index - first_index);
	index_set.shift = 0;
	return index_set;
}

std::vector<Match> match_single(const Corpus &corpus, const std::string &attr, const std::string &value)
{
	IndexSet index_set = index_lookup(corpus, attr, corpus.string2index.find(value)->second);
	std::vector<Match> matches;

	int sentence_index = 0;
	for (int token_pos : index_set.elems)
	{
		int found_sentence_start = -1;

		for (size_t i = sentence_index + 1; i < corpus.sentences.size(); i++)
		{
			if (corpus.sentences[i] > token_pos)
			{
				found_sentence_start = i;
				break;
			}
		}
		if (found_sentence_start == -1)
		{
			continue;
		}

		sentence_index = found_sentence_start - 1;

		// get the sentence start and tokens pos
		int sentence_start = corpus.sentences[sentence_index];
		int pos_in_sentence = token_pos - sentence_start;

		Match match;
		match.sentence = sentence_index;
		match.pos = pos_in_sentence;
		match.len = 1;
		matches.push_back(match);
	}

	return matches;
}

MatchSet intersection(const MatchSet &A, const MatchSet &B)
{
	MatchSet result;

	if (A.complement && B.complement)
	{
		result.set = std::visit([](auto &&a, auto &&b) -> std::variant<DenseSet, IndexSet, ExplicitSet>
								{ return intersection(a, b); }, A.set, B.set);
		result.complement = true;
		return result;
	}
	else if (A.complement)
	{
		result.set = std::visit([](auto &&a, auto &&b) -> std::variant<DenseSet, IndexSet, ExplicitSet>
								{ return difference(b, a); }, A.set, B.set);
		result.complement = false;
		return result;
	}
	else if (B.complement)
	{
		result.set = std::visit([](auto &&a, auto &&b) -> std::variant<DenseSet, IndexSet, ExplicitSet>
								{ return difference(a, b); }, A.set, B.set);
		result.complement = false;
		return result;
	}
	else
	{
		result.set = std::visit([](auto &&a, auto &&b) -> std::variant<DenseSet, IndexSet, ExplicitSet>
								{ return intersection(a, b); }, A.set, B.set);
		result.complement = false;
		return result;
	}
	return result;
}

DenseSet intersection(const DenseSet &A, const DenseSet &B)
{
	// std::cout << "Funktion 1" << std::endl;
	//  get the overlapping
	int first = std::max(A.first, B.first);
	int last = std::min(A.last, B.last);

	if (first < last)
	{
		return DenseSet{first, last};
	}
	else
	{
		// they are not overlapping
		return DenseSet{0, 0};
	}
}

ExplicitSet intersection(const ExplicitSet &A, const ExplicitSet &B)
{
	// std::cout << "Funktion 2" << std::endl;
	ExplicitSet result;

	if (A.elems.size() * SIZE_RATIO < B.elems.size())
	{
		for (int elem : A.elems)
		{
			if (std::binary_search(B.elems.begin(), B.elems.end(), elem))
			{
				result.elems.push_back(elem);
			}
		}
		return result;
	}
	else if (B.elems.size() * SIZE_RATIO < A.elems.size())
	{
		return intersection(B, A);
	}
	else
	{
		size_t p = 0, q = 0;

		while (p < A.elems.size() && q < B.elems.size())
		{
			if (A.elems[p] < B.elems[q])
			{
				++p;
			}
			else if (B.elems[q] < A.elems[p])
			{
				++q;
			}
			else
			{
				result.elems.push_back(A.elems[p]);
				++p;
				++q;
			}
		}
		return result;
	}
}

ExplicitSet intersection(const IndexSet &A, const IndexSet &B)
{
	// std::cout << "Funktion 3" << std::endl;
	ExplicitSet result;
	if (A.elems.size() * SIZE_RATIO < B.elems.size())
	{
		for (int elem : A.elems)
		{
			int shifted_elem = elem + A.shift;
			int target = shifted_elem - B.shift;

			if (std::binary_search(B.elems.begin(), B.elems.end(), target))
			{
				result.elems.push_back(shifted_elem);
			}
		}
	}
	else if (B.elems.size() * SIZE_RATIO < A.elems.size())
	{
		return intersection(B, A);
	}

	else
	{
		size_t p = 0, q = 0;
		while (p < A.elems.size() && q < B.elems.size())
		{
			int shifted_a_elem = A.elems[p] + A.shift;
			int shifted_b_elem = B.elems[q] + B.shift;

			if (shifted_a_elem < shifted_b_elem)
			{
				++p;
			}
			else if (shifted_b_elem < shifted_a_elem)
			{
				++q;
			}
			else
			{
				result.elems.push_back(shifted_a_elem);
				++p;
				++q;
			}
		}
	}

	return result;
}
ExplicitSet intersection(const DenseSet &A, const ExplicitSet &B)
{
	// std::cout << "Funktion 4" << std::endl;
	ExplicitSet result;
	for (int elem : B.elems)
	{
		// get all the elements that is in both
		if (elem >= A.first && elem < A.last)
		{
			result.elems.push_back(elem);
		}
	}
	return result;
}

IndexSet intersection(const DenseSet &A, const IndexSet &B)
{
	// std::cout << "Funktion 5" << std::endl;
	std::vector<int> result;
	for (int elem : B.elems)
	{
		int shifted_elem = elem + B.shift;
		if (shifted_elem >= A.first && shifted_elem < A.last)
		{
			result.push_back(elem);
		}
	}
	return IndexSet{std::span<int>(result.begin(), result.end()), B.shift};
}

ExplicitSet intersection(const ExplicitSet &B, const DenseSet &A)
{
	// std::cout << "Funktion 6" << std::endl;
	return intersection(A, B);
}
IndexSet intersection(const IndexSet &B, const DenseSet &A)
{
	// std::cout << "Funktion 7" << std::endl;
	return intersection(A, B);
}

ExplicitSet intersection(const ExplicitSet &A, const IndexSet &B)
{
	// std::cout << "Funktion 8" << std::endl;
	ExplicitSet result;

	if (A.elems.size() < B.elems.size() * SIZE_RATIO)
	{
		for (int elem : A.elems)
		{
			int target = elem - B.shift;
			if (std::binary_search(B.elems.begin(), B.elems.end(), target))
			{
				result.elems.push_back(elem);
			}
		}
		return result;
	}
	else if (B.elems.size() < A.elems.size() * SIZE_RATIO)
	{
		for (int elem : B.elems)
		{
			int shifted_elem = elem - B.shift;
			if (std::binary_search(A.elems.begin(), A.elems.end(), shifted_elem))
			{
				result.elems.push_back(shifted_elem);
			}
		}
		return result;
	}
	else
	{
		size_t p = 0, q = 0;

		while (p < A.elems.size() && q < B.elems.size())
		{
			if (A.elems[p] < B.elems[q] + B.shift)
			{
				++p;
			}
			else if (B.elems[q] + B.shift < A.elems[p])
			{
				++q;
			}
			else
			{
				result.elems.push_back(A.elems[p]);
				++p;
				++q;
			}
		}

		return result;
	}
}

ExplicitSet intersection(const IndexSet &B, const ExplicitSet &A)
{
	// std::cout << "Funktion 9" << std::endl;
	return intersection(A, B);
}

// for difference
ExplicitSet difference(const ExplicitSet &A, const ExplicitSet &B)
{
	// std::cout << "Funktion 10" << std::endl;
	ExplicitSet result;

	if (A.elems.size() * SIZE_RATIO < B.elems.size())
	{
		for (int elem : A.elems)
		{
			if (!std::binary_search(B.elems.begin(), B.elems.end(), elem))
			{
				result.elems.push_back(elem);
			}
		}
		return result;
	}
	else if (B.elems.size() * SIZE_RATIO < A.elems.size())
	{
		for (int elem : B.elems)
		{
			if (!std::binary_search(A.elems.begin(), A.elems.end(), elem))
			{
				result.elems.push_back(elem);
			}
		}
		return result;
	}
	else
	{
		// default algorithm
		size_t p = 0, q = 0;

		while (p < A.elems.size() && q < B.elems.size())
		{
			if (A.elems[p] < B.elems[q])
			{
				result.elems.push_back(A.elems[p]);
				++p;
			}
			else if (B.elems[q] < A.elems[p])
			{
				++q;
			}
			else
			{
				++p;
				++q;
			}
		}

		return result;
	}
}

ExplicitSet difference(const IndexSet &A, const IndexSet &B)
{
	// std::cout << "Funktion 11" << std::endl;
	ExplicitSet result;

	if (A.elems.size() * SIZE_RATIO < B.elems.size())
	{
		for (int elem : A.elems)
		{
			// apply shifts
			int shifted_elem = elem + A.shift;
			int target = shifted_elem - B.shift;

			if (!std::binary_search(B.elems.begin(), B.elems.end(), target))
			{
				result.elems.push_back(shifted_elem);
			}
		}
		return result;
	}
	else if (B.elems.size() * SIZE_RATIO < A.elems.size())
	{
		for (int elem : B.elems)
		{
			// apply shifts
			int shifted_elem = elem + B.shift;
			int target = shifted_elem - A.shift;

			if (!std::binary_search(A.elems.begin(), A.elems.end(), target))
			{
				result.elems.push_back(shifted_elem);
			}
		}
		return result;
	}
	else
	{
		size_t p = 0, q = 0;

		while (p < A.elems.size() && q < B.elems.size())
		{
			// apply the shifts
			int shifted_a_elem = A.elems[p] + A.shift;
			int shifted_b_elem = B.elems[q] + B.shift;

			if (shifted_a_elem < shifted_b_elem)
			{
				result.elems.push_back(shifted_a_elem);
				++p;
			}
			else if (shifted_b_elem < shifted_a_elem)
			{
				++q;
			}
			else
			{
				++p;
				++q;
			}
		}
		// add the rest of A
		while (p <= A.elems.size())
		{
			result.elems.push_back(A.elems[p]);
			p++;
		}

		return result;
	}
}

ExplicitSet difference(const IndexSet &A, const ExplicitSet &B)
{
	// std::cout << "Funktion 12" << std::endl;
	ExplicitSet result;

	if (A.elems.size() * SIZE_RATIO < B.elems.size())
	{
		for (int elem : A.elems)
		{
			int shifted_elem = elem + A.shift;
			if (!std::binary_search(B.elems.begin(), B.elems.end(), shifted_elem))
			{
				result.elems.push_back(shifted_elem);
			}
		}
		return result;
	}
	else if (B.elems.size() * SIZE_RATIO < A.elems.size())
	{
		for (int elem : B.elems)
		{
			if (!std::binary_search(A.elems.begin(), A.elems.end(), elem))
			{
				result.elems.push_back(elem);
			}
		}
		return result;
	}
	else
	{
		size_t p = 0, q = 0;

		while (p < A.elems.size() && q < B.elems.size())
		{
			int shifted_a_elem = A.elems[p] + A.shift;
			if (shifted_a_elem < B.elems[q])
			{
				result.elems.push_back(shifted_a_elem);
				++p;
			}
			else if (B.elems[q] < shifted_a_elem)
			{
				++q;
			}
			else
			{
				++p;
				++q;
			}
		}
		return result;
	}
}

ExplicitSet difference(const ExplicitSet &A, const IndexSet &B)
{
	// std::cout << "Function 13" << std::endl;
	ExplicitSet result;

	if (A.elems.size() * SIZE_RATIO < B.elems.size())
	{
		for (int elem : A.elems)
		{
			int target = elem - B.shift;
			// add elem in not in B
			if (!std::binary_search(B.elems.begin(), B.elems.end(), target))
			{
				result.elems.push_back(elem);
			}
		}
		return result;
	}
	else if (B.elems.size() * SIZE_RATIO < A.elems.size())
	{
		for (int elem : B.elems)
		{
			int shifted_elem = elem + B.shift;
			// add elem not in a
			if (!std::binary_search(A.elems.begin(), A.elems.end(), shifted_elem))
			{
				result.elems.push_back(shifted_elem);
			}
		}
		return result;
	}
	else
	{
		size_t p = 0;
		size_t q = 0;

		while (p < A.elems.size() && q < B.elems.size())
		{
			int shifted_b_elem = B.elems[q] + B.shift;

			if (A.elems[p] < shifted_b_elem)
			{
				// a is not in b, add to result
				result.elems.push_back(A.elems[p]);
				++p;
			}
			else if (shifted_b_elem < A.elems[p])
			{
				++q;
			}
			else
			{
				++p;
				++q;
			}
		}

		// add remaning from a
		while (p < A.elems.size())
		{
			result.elems.push_back(A.elems[p]);
			++p;
		}

		return result;
	}
}

DenseSet difference(const DenseSet &A, const DenseSet &B)
{
	// std::cout << "Funktion 14" << std::endl;
	DenseSet result{0, 0};

	// return A if there is no overlap
	if (A.last <= B.first || A.first >= B.last)
	{
		return A;
	}

	if (A.first < B.first)
	{
		// get the ovelapping part
		result.first = A.first;
		result.last = B.first;
	}
	return result;
}

ExplicitSet difference(const DenseSet &A, const ExplicitSet &B)
{
	// std::cout << "Funktion 15" << std::endl;
	ExplicitSet C;

	if (B.elems.size() > static_cast<size_t>((A.last - A.first) * SIZE_RATIO))
	{
		for (int p = A.first; p < A.last; ++p)
		{
			if (!std::binary_search(B.elems.begin(), B.elems.end(), p))
			{
				C.elems.push_back(p);
			}
		}
		return C;
	}
	else
	{
		int p = A.first;
		size_t q = 0;

		while (p < A.last && q < B.elems.size())
		{
			if (p < B.elems[q])
			{
				C.elems.push_back(p);
				p++;
			}
			else if (B.elems[q] < p)
			{
				q++;
			}
			else
			{
				p++;
				q++;
			}
		}

		while (p < A.last)
		{
			C.elems.push_back(p);
			p++;
		}

		return C;
	}
}

ExplicitSet difference(const DenseSet &A, const IndexSet &B)
{
	// std::cout << "Funktion 16" << std::endl;
	ExplicitSet result;
	if (B.elems.size() > static_cast<size_t>((A.last - A.first) * SIZE_RATIO))
	{
		for (int p = A.first; p < A.last; ++p)
		{
			// Calculate the target in B with the shift applied
			int target = p - B.shift;

			// Use binary search on B.elems to see if p (shifted) is not in B
			if (!std::binary_search(B.elems.begin(), B.elems.end(), target))
			{
				result.elems.push_back(p);
			}
		}
		return result;
	}
	else
	{

		int p = A.first;
		size_t q = 0;

		while (p < A.last && q < B.elems.size())
		{
			size_t shifted_b_elem = B.elems[q] + B.shift;

			if (p < static_cast<int>(shifted_b_elem))
			{
				result.elems.push_back(p);
				++p;
			}
			else if (static_cast<int>(shifted_b_elem) < p)
			{
				++q;
			}
			else
			{
				++p;
				++q;
			}
		}
		// get the remaning elements
		while (p <= A.last)
		{
			result.elems.push_back(p);
			++p;
		}

		return result;
	}
}

ExplicitSet difference(const ExplicitSet &B, const DenseSet &A)
{
	// std::cout << "Funktion 17" << std::endl;
	ExplicitSet C;
	int p = 0;
	int q = A.first;

	while (p < static_cast<int>(B.elems.size()) && q < static_cast<int>(A.last))
	{
		if (B.elems[p] < q)
		{
			C.elems.push_back(B.elems[p]);
			p++;
		}
		else if (q < B.elems[p])
		{
			q++;
		}
		else
		{
			p++;
			q++;
		}
	}

	while (p < static_cast<int>(B.elems.size()))
	{
		C.elems.push_back(B.elems[p]);
		p++;
	}

	return C;
}

ExplicitSet difference(const IndexSet &B, const DenseSet &A)
{
	// std::cout << "Funktion 18" << std::endl;
	ExplicitSet C;
	size_t p = 0;
	size_t q = A.first;

	while (p < B.elems.size() && q < static_cast<size_t>(A.last))
	{
		int shifted_elem = B.elems[p] + B.shift;

		if (static_cast<size_t>(shifted_elem) < q)
		{
			C.elems.push_back(shifted_elem);
			p++;
		}
		else if (q < static_cast<size_t>(shifted_elem))
		{
			q++;
		}
		else
		{
			p++;
			q++;
		}
	}

	while (p < B.elems.size())
	{
		C.elems.push_back(B.elems[p] + B.shift);
		p++;
	}

	return C;
}

MatchSet match_set(const Corpus &corpus, const Literal &literal, int shift)
{
	MatchSet result;

	const std::string &attribute = literal.attribute;
	uint32_t value = literal.value;
	// get the index set
	IndexSet index_set = index_lookup(corpus, attribute, value);
	index_set.shift = shift;

	if (!literal.is_equality)
	{
		result.complement = true;
	}
	else
	{
		result.complement = false;
	}
	result.set = index_set;

	return result;
}

void match_set(const Corpus &corpus, const Clause &clause, int shift, std::vector<MatchSet> &sets, bool &dense_sets)
{

	if (clause[0].attribute == "match all")
	{
		// dense set appared
		dense_sets = true;
	}
	else
	{
		MatchSet result;

		for (const Literal &literal : clause)
		{
			MatchSet literalMatchSet = match_set(corpus, literal, shift);
			sets.push_back(literalMatchSet);
		}
	}
}

size_t get_set_size(const MatchSet &set)
{
	return std::visit([](auto &&s) -> size_t
					  {
        using T = std::decay_t<decltype(s)>;
        if constexpr (std::is_same_v<T, DenseSet>) {
            return s.last - s.first;
        } else if constexpr (std::is_same_v<T, IndexSet>) {
            return s.elems.size();
        } else if constexpr (std::is_same_v<T, ExplicitSet>) {
            return s.elems.size();
        } }, set.set);
}

bool compare_size(const MatchSet &A, const MatchSet &B)
{
	size_t size_A = get_set_size(A);
	size_t size_B = get_set_size(B);
	return size_A < size_B;
}

MatchSet match_set(const Corpus &corpus, const Query &query)
{
	MatchSet result;
	std::vector<MatchSet> sets;
	bool dense_sets;

	int shift = 0;

	for (const Clause &clause : query)
	{
		match_set(corpus, clause, shift, sets, dense_sets);
		shift--;
	}

	if (!sets.empty())
	{

		std::sort(sets.begin(), sets.end(), compare_size);

		result = sets[0];
		for (size_t i = 1; i < sets.size(); i++)
		{
			result = intersection(sets[i], result);
		}
	}

	if (dense_sets)
	{
		// atleast one dense set
		int size = corpus.tokens.size() - 1;
		DenseSet empty_set{0, size};
		MatchSet empty;
		empty.set = empty_set;
		empty.complement = false;
		result = intersection(empty, result);
	}

	if (result.complement)
	{
		// we need to flip this
		int size = corpus.tokens.size() - 1;
		DenseSet empty_set{0, size};
		MatchSet empty;
		empty.set = empty_set;
		empty.complement = false;
		return intersection(empty, result);
	}
	return result;
}

SentencePosition find_sentence_position_and_check(const Corpus &corpus, int pos, int matchLength)
{
	auto it = std::upper_bound(corpus.sentences.begin(), corpus.sentences.end(), pos);
	if (it == corpus.sentences.begin())
	{
		// match in first sentence
		return {0, pos, pos + matchLength <= *it};
	}
	if (it == corpus.sentences.end())
	{
		// match in last sentence
		int sentence_index = corpus.sentences.size() - 1;
		return {sentence_index, pos - corpus.sentences[sentence_index], true};
	}
	// the index of the sentence
	int sentence_index = std::distance(corpus.sentences.begin(), it) - 1;
	// get pos in sentence
	int position_in_sentence = pos - corpus.sentences[sentence_index];
	bool in_same_sentence;
	if (pos + matchLength <= *it)
	{
		in_same_sentence = true;
	}
	else
	{
		in_same_sentence = false;
	}
	return {sentence_index, position_in_sentence, in_same_sentence};
}

std::vector<Match> match2(const Corpus &corpus, const Query &query)
{
	MatchSet matchSet = match_set(corpus, query);
	std::vector<Match> matches;
	int matchLenght = query.size();
	std::visit([&](auto &&set)
			   {
        using T = std::decay_t<decltype(set)>;

        if constexpr (std::is_same_v<T, DenseSet>) {
          	for (int pos = set.first; pos < set.last; ++pos) {
                auto result = find_sentence_position_and_check(corpus, pos, matchLenght);
                if (result.is_valid) {
                    matches.push_back(Match{result.sentence_index, result.position_in_sentence, matchLenght});
                }
            }

        } else if constexpr (std::is_same_v<T, IndexSet>) {
            for (int pos : set.elems) {
                int shifted_pos = pos + set.shift;
                auto result = find_sentence_position_and_check(corpus, shifted_pos, matchLenght);
                if (result.is_valid) {
                    matches.push_back(Match{result.sentence_index, result.position_in_sentence, matchLenght});
                }
            }
        } else if constexpr (std::is_same_v<T, ExplicitSet>) {
            for (int pos : set.elems) {
                auto result = find_sentence_position_and_check(corpus, pos, matchLenght);
                if (result.is_valid) {
                    matches.push_back(Match{result.sentence_index, result.position_in_sentence, matchLenght});
                }
            }
        } }, matchSet.set);

	return matches;
}
