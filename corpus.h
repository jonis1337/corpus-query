#ifndef CORPUS_H
#define CORPUS_H

#include <string>
#include <vector>
#include <map>
#include <span>
#include <iterator>
#include <variant>

struct Token
{
	uint32_t word;
	uint32_t c5;
	uint32_t lemma;
	uint32_t pos;
};
struct Literal
{
	std::string attribute; // left-hand side
	uint32_t value;		   // right-hand side
	bool is_equality;	   // true if = and false if !=
};
using Index = std::vector<int>;
struct Corpus
{
	std::vector<Token> tokens;
	std::vector<int> sentences;
	std::vector<std::string> index2string;
	std::map<std::string, uint32_t> string2index;
	Index word_index;  // NEW
	Index c5_index;	   // NEW
	Index lemma_index; // NEW
	Index pos_index;   // NEW
};
using Clause = std::vector<Literal>;
using Query = std::vector<Clause>;
struct IndexSet
{
	std::span<const int> elems;
	int shift; // NEW
};

struct DenseSet
{
	int first;
	int last;
};

struct ExplicitSet
{
	std::vector<int> elems;
};

struct MatchSet
{
	std::variant<DenseSet, IndexSet, ExplicitSet> set;
	bool complement;
};

struct Match
{
	int sentence;
	int pos;
	int len;
};
struct SentencePosition
{
	int sentence_index;
	int position_in_sentence;
	bool is_valid;
};

Corpus load_corpus(const std::string &filename);
std::vector<Match> match(const Corpus &corpus, const std::string &query_string);
Query parse_query(const std::string &text, const Corpus &corpus);
std::vector<Match> match(const Corpus &corpus, const Query &query);
void print_matches(const Corpus &corpus, const std::vector<Match> &matches);
Index build_index(const std::vector<Token> &tokens, uint32_t Token::*attribute);
void build_indices(Corpus &corpus);
IndexSet index_lookup(const Corpus &corpus, const std::string &attribute, uint32_t value);
std::vector<Match> match_single(const Corpus &corpus, const std::string &attr, const std::string &value);
MatchSet intersection(const MatchSet &A, const MatchSet &B);
// helper functions
DenseSet intersection(const DenseSet &A, const DenseSet &B);
ExplicitSet intersection(const DenseSet &A, const ExplicitSet &B);
IndexSet intersection(const DenseSet &A, const IndexSet &B);
ExplicitSet intersection(const ExplicitSet &B, const DenseSet &A);
IndexSet intersection(const IndexSet &B, const DenseSet &A);

ExplicitSet intersection(const ExplicitSet &A, const ExplicitSet &B);
ExplicitSet intersection(const ExplicitSet &A, const IndexSet &B);

ExplicitSet intersection(const IndexSet &B, const ExplicitSet &A);
ExplicitSet intersection(const IndexSet &A, const IndexSet &B);

DenseSet difference(const DenseSet &A, const DenseSet &B);
ExplicitSet difference(const DenseSet &A, const ExplicitSet &B);
ExplicitSet difference(const DenseSet &A, const IndexSet &B);
ExplicitSet difference(const ExplicitSet &B, const DenseSet &A);
ExplicitSet difference(const IndexSet &B, const DenseSet &A);

ExplicitSet difference(const ExplicitSet &A, const ExplicitSet &B);
ExplicitSet difference(const ExplicitSet &A, const IndexSet &B);
ExplicitSet difference(const IndexSet &A, const ExplicitSet &B);
ExplicitSet difference(const IndexSet &A, const IndexSet &B);

size_t get_set_size(const MatchSet &set);
std::vector<Match> match2(const Corpus &corpus, const Query &query);

#endif // CORPUS_H