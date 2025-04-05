# Annotated Text Corpus Query Tool

## Table of Contents
1. [Project Overview](#project-overview)
2. [Corpus File Format](#corpus-file-format)
3. [Corpus Query Format](#corpus-query-format)
4. [Installation](#installation)

## Project Overview

This project is a simple tool for querying annotated text corpora, inspired by the paper *"Efficient corpus search using unary and binary indexes"* by Peter Ljunglöf and Nicholas Smallbone. An annotated text corpus is a collection of sentences where each token (word, punctuation, etc.) is annotated with attributes such as its lemma (dictionary form) and part-of-speech (POS) tag (e.g., noun, verb, adjective). These annotations enable advanced queries beyond simple string matching, such as finding all instances of an adjective followed by a form of the noun "house" (e.g., "red house," "big houses").

The tool starts with basic data structures and algorithms and progressively incorporates more sophisticated techniques, such as unary and binary indexes, to optimize query performance as described in the referenced paper.

### Example Queries
- Find all occurrences of an adjective followed by a form of "house":
  - Matches: "… steals fuel to warm his own house in Meknes." or "… collections by the provisionals in public houses were having partial success …"

## Corpus File Format
The corpus is stored in a text-based, line-oriented format with the following rules:
- Lines starting with `#` are comments and ignored.
- Empty lines separate sentences and are ignored.
- Each remaining line represents a token with four tab-separated attributes:
  - **word**: The actual word as it appears in the text.
  - **c5**: A fine-grained word class tag.
  - **lemma**: The lemmatized (dictionary) form of the word.
  - **pos**: A coarse-grained part-of-speech tag (e.g., `SUBST` for noun, `ADJ` for adjective).

#### Supported POS Tags
- `SUBST`: Noun (e.g., house, cat)
- `ADJ`: Adjective (e.g., red, small)
- `VERB`: Verb (e.g., be, travel)
- `ADV`: Adverb (e.g., quickly, carefully)
- `ART`: Article (e.g., a, an, the)
- `PREP`: Preposition (e.g., in, under)
- `PRON`: Pronoun (e.g., he, she)
- `CONJ`: Conjunction (e.g., and, but)
- `INTERJ`: Interjection (e.g., Yes!, Oh!)
- `PUL`: Left parenthesis `(`
- `PUR`: Right parenthesis `)`
- `PUN`: Punctuation (e.g., `.`, `?`)
- `PUQ`: Quotation mark
- `UNC`: Unknown

#### Example Corpus Excerpt
 ```
# sentence 13, Texts/A/A0/A00.xml
Did     VDD     do      VERB
you     PNP     you     PRON
know    VVI     know    VERB
?       PUN     ?       PUN

# sentence 14, Texts/A/A0/A00.xml
there   EX0     there   PRON
is      VBZ     be      VERB
no      AT0     no      ART
vaccine NN1     vaccine SUBST
or      CJC     or      CONJ
cure    VVB-NN1 cure    VERB
currently       AV0     currently       ADV
available       AJ0     available       ADJ
.       PUN     .       PUN

```

## Corpus Query Format
We first need to define the query language grammar and semantics.

-   A *query* is a non-empty sequence of clauses.
-   A *clause* is a possibly empty sequence of literals.
-   A *literal* is either `attr=value` or `attr!=value`, where `attr` is one of the attributes in the corpus (i.e., word, c5, lemma, or pos).
-   A *value* is a "-quoted string (e.g., `"house"`).
-   A *string* is any sequence of characters excluding " (e.g., `house`).

In extended Backus-Naur form, the grammar for the query language can be formally expressed as follows:
```
<query>     ::= <clause> { <clause> }
<clause>    ::= '[' , { <literal> } , ']'
<literal>   ::= <attribute> , ( '=' | '!=' ) , <value>
<attribute> ::= 'word' | 'c5' | 'lemma' | 'pos'
<value>     ::= '"' , <string> , '"'
```
Not shown in the grammar (for simplicity) is that whitespace can be inserted between any tokens, but whitespace is only required to separate literals.

Here are examples of valid queries:
```
[]
[lemma="house"]
[lemma="house" pos!="VERB"]
[pos="ART"] [lemma="house"]
```
The semantics of a query is as follows.

-   A query matches a contiguous sequence of tokens (one per clause).
-   A match cannot cross a sentence boundary.
-   Each clause matches exactly one token.
-   For a token to match a clause, the token must match all of its literals.
-   A literal of the form `attr="value"` matches if the token's attribute `attr` is `value`. A literal of the form `attr!="value"` matches if the token's attribute `attr` is not `value`.
-   The empty clause matches any token.
-   Matches are case-sensitive.
 For example, the query `[pos="ART"] [lemma="house"]` matches any adjacent pair of tokens A B where the `pos`attribute of A is `ART` and the `lemma` attribute of B is `house`

 ## Installation
1. **Clone the repository**
   ```bash
   git clone https://github.com/jonis1337/corpus-query.git
   cd corpus-query
   ```
2. **Unzip the bnc-05M.csv text corpa**
   ```bash
   unzip  bnc-05M.csv.zip
   ```
3. **Compile**
    ```bash
    make
    ```
4. **Run**
    ```
    ./corpus bnc-05M.csv
    ```
5. **Sample query**
    ```
    Enter a query (or press Enter to exit): [lemma="house"]    
    ```

