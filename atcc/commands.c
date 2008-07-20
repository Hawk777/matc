#include "commands.h"
#include <string.h>



#define MAX_FOLLOWING_FRAGMENT_LISTS 5

/* Represents a particular fragment of a command. A "fragment" refers to a single letter in the input.
 * A "fragment list" means an array of struct fragments the last of which has input set to NUL and output set to NULL.
 */
struct fragment;
struct fragment {
	/* The input letter corresponding to this fragment. */
	char input;

	/* The output text corresponding to this fragment. */
	const char *output;

	/* Whether it is permitted to hit ENTER here. */
	int terminal;

	/* Pointers to fragment lists that are permitted to follow this fragment (terminate with a null pointer). */
	const struct fragment *followers[MAX_FOLLOWING_FRAGMENT_LISTS];
};



/* Landmark ID numbers (terminal) */
static const struct fragment landmarks_terminal[] = {
	{'0', "0", 1, {0}},
	{'1', "1", 1, {0}},
	{'2', "2", 1, {0}},
	{'3', "3", 1, {0}},
	{'4', "4", 1, {0}},
	{'5', "5", 1, {0}},
	{'6', "6", 1, {0}},
	{'7', "7", 1, {0}},
	{'8', "8", 1, {0}},
	{'9', "9", 1, {0}},
	{'\0', 0, 0, {0}},
};

/* Delay targets */
static const struct fragment delay_targets[] = {
	{'b', " beacon #", 0, {landmarks_terminal, 0}},
	{'*', " beacon #", 0, {landmarks_terminal, 0}},
	{'\0', 0, 0, {0}}
};

/* The delay command */
static const struct fragment delay[] = {
	{'a', " at", 0, {delay_targets, 0}},
	{'\0', 0, 0, {0}}
};

/* Absolute altitude numbers */
static const struct fragment altnum_absolute[] = {
	{'0', " 0000 feet", 1, {0}},
	{'1', " 1000 feet", 1, {0}},
	{'2', " 2000 feet", 1, {0}},
	{'3', " 3000 feet", 1, {0}},
	{'4', " 4000 feet", 1, {0}},
	{'5', " 5000 feet", 1, {0}},
	{'6', " 6000 feet", 1, {0}},
	{'7', " 7000 feet", 1, {0}},
	{'8', " 8000 feet", 1, {0}},
	{'9', " 9000 feet", 1, {0}},
	{'\0', 0, 0, {0}}
};

/* Relative altitude numbers */
static const struct fragment altnum_relative[] = {
	{'0', " 0000 ft", 1, {0}},
	{'1', " 1000 ft", 1, {0}},
	{'2', " 2000 ft", 1, {0}},
	{'3', " 3000 ft", 1, {0}},
	{'4', " 4000 ft", 1, {0}},
	{'5', " 5000 ft", 1, {0}},
	{'6', " 6000 ft", 1, {0}},
	{'7', " 7000 ft", 1, {0}},
	{'8', " 8000 ft", 1, {0}},
	{'9', " 9000 ft", 1, {0}},
	{'\0', 0, 0, {0}}
};

/* Climb and descend modifiers to Altitude */
static const struct fragment altmods[] = {
	{'c', " climb", 0, {altnum_relative, 0}},
	{'+', " climb", 0, {altnum_relative, 0}},
	{'d', " descend", 0, {altnum_relative, 0}},
	{'-', " descend", 0, {altnum_relative, 0}},
	{'\0', 0, 0, {0}},
};

/* Absolute directions */
static const struct fragment directions_absolute[] = {
	{'q', " to 315", 1, {delay, 0}},
	{'w', " to 0", 1, {delay, 0}},
	{'e', " to 45", 1, {delay, 0}},
	{'a', " to 270", 1, {delay, 0}},
	{'d', " to 90", 1, {delay, 0}},
	{'z', " to 225", 1, {delay, 0}},
	{'x', " to 180", 1, {delay, 0}},
	{'c', " to 135", 1, {delay, 0}},
	{'\0', 0, 0, {0}},
};

/* Relative directions */
static const struct fragment directions_relative[] = {
	{'q', " 315", 1, {delay, 0}},
	{'w', " 0", 1, {delay, 0}},
	{'e', " 45", 1, {delay, 0}},
	/* NOTE! A is not present here because it means At instead of 270! */
	{'d', " 90", 1, {delay, 0}},
	{'z', " 225", 1, {delay, 0}},
	{'x', " 180", 1, {delay, 0}},
	{'c', " 135", 1, {delay, 0}},
	{'\0', 0, 0, {0}},
};

/* Single-character sharp turn angles */
static const struct fragment turns_sharp[] = {
	{'L', " left 90", 1, {delay, 0}},
	{'R', " right 90", 1, {delay, 0}},
	{'\0', 0, 0, {0}}
};

/* Moderate turns that accept optional angle parameters */
static const struct fragment turns_normal[] = {
	{'l', " left", 1, {delay, directions_relative, 0}},
	{'r', " right", 1, {delay, directions_relative, 0}},
	{'\0', 0, 0, {0}}
};

/* Landmark ID numbers (delayable) */
static const struct fragment landmarks_delayable[] = {
	{'0', "0", 1, {delay, 0}},
	{'1', "1", 1, {delay, 0}},
	{'2', "2", 1, {delay, 0}},
	{'3', "3", 1, {delay, 0}},
	{'4', "4", 1, {delay, 0}},
	{'5', "5", 1, {delay, 0}},
	{'6', "6", 1, {delay, 0}},
	{'7', "7", 1, {delay, 0}},
	{'8', "8", 1, {delay, 0}},
	{'9', "9", 1, {delay, 0}},
	{'\0', 0, 0, {0}},
};

/* Turns towards landmarks */
static const struct fragment turn_landmarks[] = {
	{'a', " airport #", 0, {landmarks_delayable, 0}},
	{'b', " beacon #", 0, {landmarks_delayable, 0}},
	{'e', " exit #", 0, {landmarks_delayable, 0}},
	{'*', " beacon #", 0, {landmarks_delayable, 0}},
	{'\0', 0, 0, {0}}
};

/* The word Towards */
static const struct fragment towards[] = {
	{'t', " towards", 0, {turn_landmarks, 0}},
	{'\0', 0, 0, {0}}
};

/* The set of all commands */
static const struct fragment commands[] = {
	{'a', " altitude:", 0, {altnum_absolute, altmods, 0}},
	{'m', " mark", 1, {0}},
	{'i', " ignore", 1, {0}},
	{'u', " unmark", 1, {0}},
	{'c', " circle", 1, {0}},
	{'t', " turn", 0, {turns_sharp, turns_normal, directions_absolute, towards, 0}},
	{'\0', 0, 0, {0}}
};

/* The set of all plane letters */
static const struct fragment planes[] = {
	{'a', "a:", 0, {commands, 0}},
	{'b', "b:", 0, {commands, 0}},
	{'c', "c:", 0, {commands, 0}},
	{'d', "d:", 0, {commands, 0}},
	{'e', "e:", 0, {commands, 0}},
	{'f', "f:", 0, {commands, 0}},
	{'g', "g:", 0, {commands, 0}},
	{'h', "h:", 0, {commands, 0}},
	{'i', "i:", 0, {commands, 0}},
	{'j', "j:", 0, {commands, 0}},
	{'k', "k:", 0, {commands, 0}},
	{'l', "l:", 0, {commands, 0}},
	{'m', "m:", 0, {commands, 0}},
	{'n', "n:", 0, {commands, 0}},
	{'o', "o:", 0, {commands, 0}},
	{'p', "p:", 0, {commands, 0}},
	{'q', "q:", 0, {commands, 0}},
	{'r', "r:", 0, {commands, 0}},
	{'s', "s:", 0, {commands, 0}},
	{'t', "t:", 0, {commands, 0}},
	{'u', "u:", 0, {commands, 0}},
	{'v', "v:", 0, {commands, 0}},
	{'w', "w:", 0, {commands, 0}},
	{'x', "x:", 0, {commands, 0}},
	{'y', "y:", 0, {commands, 0}},
	{'z', "z:", 0, {commands, 0}},
	{'A', "A:", 0, {commands, 0}},
	{'B', "B:", 0, {commands, 0}},
	{'C', "C:", 0, {commands, 0}},
	{'D', "D:", 0, {commands, 0}},
	{'E', "E:", 0, {commands, 0}},
	{'F', "F:", 0, {commands, 0}},
	{'G', "G:", 0, {commands, 0}},
	{'H', "H:", 0, {commands, 0}},
	{'I', "I:", 0, {commands, 0}},
	{'J', "J:", 0, {commands, 0}},
	{'K', "K:", 0, {commands, 0}},
	{'L', "L:", 0, {commands, 0}},
	{'M', "M:", 0, {commands, 0}},
	{'N', "N:", 0, {commands, 0}},
	{'O', "O:", 0, {commands, 0}},
	{'P', "P:", 0, {commands, 0}},
	{'Q', "Q:", 0, {commands, 0}},
	{'R', "R:", 0, {commands, 0}},
	{'S', "S:", 0, {commands, 0}},
	{'T', "T:", 0, {commands, 0}},
	{'U', "U:", 0, {commands, 0}},
	{'V', "V:", 0, {commands, 0}},
	{'W', "W:", 0, {commands, 0}},
	{'X', "X:", 0, {commands, 0}},
	{'Y', "Y:", 0, {commands, 0}},
	{'Z', "Z:", 0, {commands, 0}},
	{'\0', 0, 0, {0}}
};

/* The root metalist. */
const struct fragment *root[] = {
	planes,
	0
};



static const struct fragment *find_fragment(const struct fragment * const *fraglist, char input) {
	unsigned int i, j;

	for (i = 0; i < MAX_FOLLOWING_FRAGMENT_LISTS && fraglist[i]; i++)
		for (j = 0; fraglist[i][j].input; j++)
			if (fraglist[i][j].input == input)
				return &fraglist[i][j];
	return 0;
}



int parse_command(const char *readptr, char *writeptr, size_t writelen, int *terminal) {
	const struct fragment * const *fraglist = root;
	const struct fragment *fragptr = 0;

	/* An empty buffer is considered an error. */
	if (!writelen)
		return -1;

	/* An empty string is actually considered acceptable and terminal. */
	*writeptr = '\0';
	if (!*readptr) {
		*terminal = 1;
		return 0;
	}

	/* Iterate the characters in the input. */
	while (*readptr) {
		/* Given the current set of acceptable fragment lists, try to find a fragment matching the input char. */
		fragptr = find_fragment(fraglist, *readptr++);
		/* If we didn't find any such fragment, give up. */
		if (!fragptr)
			return -1;
		/* If there's not enough buffer space left, give up. */
		if (strlen(writeptr) + strlen(fragptr->output) + 1 > writelen)
			return -1;
		/* Append the found fragment's description to the output buffer. */
		strcat(writeptr, fragptr->output);
		/* Look for the next character in the followers list of the found fragment. */
		fraglist = fragptr->followers;
	}

	/* We got to the end of the input, which means we're successful. */
	*terminal = fragptr->terminal;
	return 0;
}

