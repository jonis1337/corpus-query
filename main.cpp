#include <iostream>
#include <chrono>
#include <fstream>
#include "corpus.h"

enum class state
{
	attribute,
	value,
	equality,
	expect_close
};

double benchmark_query(const Corpus &corpus, const Query &query, size_t total_tokens, int num_runs, int run)
{
	double total_time_s = 0.0;
	std::vector<Match> result;

	// warmup
	for (int i = 0; i < 100; ++i)
	{
		match2(corpus, query);
	}

	// benchmark runs
	for (int i = 0; i < num_runs; ++i)
	{
		auto start = std::chrono::high_resolution_clock::now();

		result = match2(corpus, query);

		auto end = std::chrono::high_resolution_clock::now();
		std::chrono::duration<double> elapsed = end - start;
		total_time_s += elapsed.count();
	}

	double avg_time_s = total_time_s / num_runs;
	double tokens_per_second = total_tokens / avg_time_s;

	std::ofstream outfile;
	// open in append mode
	outfile.open("doubble.txt", std::ios_base::app);

	outfile << "Query Run: " << run << std::endl;
	outfile << "Matches: " << result.size() << std::endl;
	outfile << "Average Time: " << avg_time_s << " s" << std::endl;
	outfile << "Tokens Processed per Second: " << tokens_per_second << std::endl;
	outfile << "----------------------------------------" << std::endl;

	outfile.close();

	return avg_time_s;
}

int main()
{
	// clear terminal
	std::cout << "\033[H\033[2J" << std::endl;
	std::cout << "\033[1;34mLoading Corpus...\033[0m" << std::endl;

	Corpus corpus = load_corpus("doubble.csv");
	build_indices(corpus);

	/*uint32_t index = corpus.string2index["bodybuilder"];
	uint32_t index2 = corpus.string2index["bodybuilders"];
	IndexSet hej = index_lookup(corpus, "lemma", index);
	IndexSet da = index_lookup(corpus, "word", index2);
	MatchSet match1;
	match1.set = hej;
	match1.complement = false;

	MatchSet match2;
	match2.set = da;
	match2.complement = true;
	MatchSet result;
	result = intersection(match1, match2);

	Token tok = corpus.tokens[913814];

	std::cout << corpus.index2string[tok.word] << std::endl;

	match_single(corpus, "lemma", "hamburger");
	*/
	// clear
	// std::cout << corpus.string2index.size() << corpus.index2string.size() << std::endl;
	std::string text = "";
	std::cout << "\033[H\033[2J" << std::endl;
	while (true)
	{
		std::cout << "Enter a query (or press Enter to exit): ";
		std::getline(std::cin, text);
		if (text.empty())
		{
			std::cout << "\033[H\033[2J" << std::endl;
			break;
		}
		if (text == "bench")
		{
			// run benchmarks
			std::cout << "Running benchmarks..." << std::endl;
			std::cout << corpus.tokens.size() << std::endl;
			const std::string pattern1 = "[lemma=\"a\"]";
			const std::string pattern2 = "[lemma=\"house\" word!=\"House\" pos=\"SUBST\"][]";
			const std::string pattern3 = "[word=\"Nothing\"][][][lemma!=\"be\"][][word=\"palmtrees\" lemma=\"palmtree\"][word!=\"way\"][][word=\"And\"]";

			double avg_time1 = benchmark_query(corpus, parse_query(pattern1, corpus), corpus.tokens.size(), 1000, 1);
			double avg_time2 = benchmark_query(corpus, parse_query(pattern2, corpus), corpus.tokens.size(), 1000, 2);
			double avg_time3 = benchmark_query(corpus, parse_query(pattern3, corpus), corpus.tokens.size(), 1000, 3);

			double avg_time = (avg_time1 + avg_time2 + avg_time3) / 3;
			// append the average to the file also
			std::ofstream outfile;
			outfile.open("doubble.txt", std::ios_base::app);
			outfile << "Overall Average Time (for 3 queries): " << avg_time << " s" << std::endl;
			outfile << "----------------------------------------" << std::endl;
			outfile.close();

			std::cout << "Benchmark done! Overall Average Time: " << avg_time << " s" << std::endl;
		}

		// text = "[lemma=\"house\" pos!=\"VERB\"]";
		try
		{
			Query query = parse_query(text, corpus);
			std::vector<Match> matches = match2(corpus, query);
			print_matches(corpus, matches);
		}
		catch (const std::exception &e)
		{
			std::cerr << "Query syntax error:" << e.what() << '\n';
		}
	}
	return 0;
}

Query parse_query(const std::string &text, const Corpus &corpus)
{
	state current_state = state::attribute;
	Query query;
	Clause clause;
	Literal literal;

	std::string attribute;
	std::string value;

	// get th iterator
	auto iter = corpus.string2index.end();

	size_t i = 0;
	while (i < text.size())
	{
		switch (current_state)
		{
		case state::attribute:
			if (text[i] == '[')
			{
				// start with empty clause
				clause.clear();
				literal = Literal();
				// move to the next char
				i++;
				if (i < text.size() && text[i] == ']')
				{
					// empty clause
					i++;
					literal.attribute = "match all";
					clause.push_back(literal);
					query.push_back(clause);
					current_state = state::attribute;
					while (isspace(text[i]))
						i++;
					break;
				}
			}
			else
			{
				// parse the attribute
				attribute.clear();
				while (i < text.size() && std::isalpha(text[i]))
				{
					attribute += text[i];
					i++;
				}
				if (attribute.empty())
				{
					throw std::runtime_error("Error: expected an attribute");
				}

				literal.attribute = attribute;
				// now we expect an equality sign
				current_state = state::equality;
				break;
			}
			break;

		case state::equality:
			if (i >= text.size())
			{
				throw std::runtime_error("Error: can't end with an equality or inequalitysign");
			}
			else if (i < text.size() && text[i] == '=')
			{
				literal.is_equality = true;
				i++;
				current_state = state::value;
			}
			else if (i < text.size() && text[i] == '!')
			{
				i++;
				if (i >= text.size() || text[i] != '=')
				{
					throw std::runtime_error("Error: expected '=' after '!'");
				}
				literal.is_equality = false;
				i++;
				// now we expect a value
				current_state = state::value;
			}
			else
			{
				throw std::runtime_error("Error: expected '=' or '!='");
			}
			break;

		case state::value:
			if (i >= text.size() || text[i] != '"')
			{
				throw std::runtime_error("Error: expected opening: \" for value");
			}
			i++;

			value.clear();
			while (i < text.size() && text[i] != '"')
			{
				value += text[i];
				i++;
			}

			if (i >= text.size() || text[i] != '"')
			{
				throw std::runtime_error("Error: expected closing: \" for value");
			}

			iter = corpus.string2index.find(value);

			if (iter != corpus.string2index.end())
			{
				// already in map return index
				literal.value = iter->second;
			}
			else
			{
				// this query will not match so we give it a number that does not exist
				uint32_t id = -1;
				literal.value = id;
			}

			i++;
			// now we expect a space or a closing bracket
			current_state = state::expect_close;
			break;

		case state::expect_close:
			if (i < text.size() && text[i] == ' ')
			{
				// add the literal to current clause
				clause.push_back(literal);
				literal = Literal();
				i++;
				current_state = state::attribute;
			}
			else if (i <= text.size() && text[i] == ']')
			{
				// add the literal to current clause and add the clause to the query
				clause.push_back(literal);
				query.push_back(clause);
				clause.clear();
				literal = Literal();
				i++;
				if (i < text.size() && text[i] == ' ')
				{
					// one space is allowed
					i++;
				}
				current_state = state::attribute;
			}
			else
			{
				throw std::runtime_error("Error: expected space or ]");
			}
			break;

		default:
			throw std::runtime_error("Error: something went wrong");
		}
	}

	return query;
}

void print_tokens(const Corpus &corpus, const Match &match)
{
	size_t index = match.sentence;

	if (index + 1 > corpus.sentences.size())
	{
		std::cout << "OUT OF BOUNDS" << std::endl;
		std::cout << index << "  " << corpus.sentences.size() << std::endl;
		return;
	}

	int sentence_lenght = corpus.sentences[index + 1] - corpus.sentences[index];
	int start = corpus.sentences[index];
	int end = start + sentence_lenght;
	// std::cout << "Match at position: " << match.pos << " with length: " << match.len << std::endl;
	int token_numb = 0;
	int lenght = 0;
	bool highlight_token = false;

	while (start != end)
	{
		const Token &token = corpus.tokens[start];
		if (token_numb == match.pos)
		{
			highlight_token = true;
		}
		if (highlight_token)
		{
			if (lenght < match.len)
			{
				std::cout << "\033[1;37;43m";
				lenght++;
			}
			else
			{
				std::cout << "\033[0m";
				highlight_token = false;
			}
		}
		else
		{
			std::cout << "\033[0m";
		}
		std::cout << corpus.index2string[token.word] << "\033[0m ";
		token_numb++;
		start++;
	}
	std::cout << std::endl
			  << std::endl;
}

void print_matches(const Corpus &corpus, const std::vector<Match> &matches)
{
	// std::cout << corpus.tokens.size() << std::endl;
	// std::cout << corpus.sentences.size() << std::endl;
	// std::cout << matches.size() << std::endl;
	if (matches.size() > 10)
	{
		std::cout << std::endl
				  << "Listing first 10 matches:" << std::endl;
		for (int i = 0; i < 10; i++)
		{
			const Match &match = matches[i];
			print_tokens(corpus, match);
		}
	}
	else if (matches.size() > 0)
	{
		std::cout << std::endl
				  << "Listing " << matches.size() << " matches:" << std::endl;

		for (size_t i = 0; i < matches.size(); i++)
		{
			const Match &match = matches[i];
			print_tokens(corpus, match);
		}
	}
	else
	{
		std::cout << "No matches found." << std::endl;
	}

	if (matches.size() > 0)
	{
		std::cout << "------ Total matches: " << matches.size() << " ------" << std::endl;
	}
}