#include "Expand.hxx"

#include <gtest/gtest.h>

using namespace std::string_view_literals;

TEST(Expand, ReplacesWholeStringVariable)
{
	StringMap vars{{"PLAN"sv, "backup"sv}};

	std::string value = "${PLAN}";
	Expand(value, vars);

	EXPECT_EQ(value, "backup");
}

TEST(Expand, PreservesMissingVariableAsEmptyString)
{
	const StringMap vars;

	std::string value = "prefix-${MISSING}-suffix";
	Expand(value, vars);

	EXPECT_EQ(value, "prefix--suffix");
}

TEST(Expand, ReplacesEmbeddedVariable)
{
	StringMap vars{
		{"JOB"sv, "42"sv},
		{"PLAN"sv, "nightly"sv},
	};

	std::string value = "prefix-${JOB}-${PLAN}-suffix";
	Expand(value, vars);

	EXPECT_EQ(value, "prefix-42-nightly-suffix");
}

TEST(Expand, LeavesUnterminatedPlaceholderLiteral)
{
	StringMap vars{{"JOB"sv, "42"sv}};

	std::string value = "prefix-${JOB";
	Expand(value, vars);

	EXPECT_EQ(value, "prefix-${JOB");
}
