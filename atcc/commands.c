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
	bool terminal;

	/* Pointers to fragment lists that are permitted to follow this fragment (terminate with a null pointer). */
	const struct fragment *followers[MAX_FOLLOWING_FRAGMENT_LISTS];
};



/* Landmark ID numbers (terminal) */
static const struct fragment landmarks_terminal[] = {
	{'0', "0", true, {nullptr}},
	{'1', "1", true, {nullptr}},
	{'2', "2", true, {nullptr}},
	{'3', "3", true, {nullptr}},
	{'4', "4", true, {nullptr}},
	{'5', "5", true, {nullptr}},
	{'6', "6", true, {nullptr}},
	{'7', "7", true, {nullptr}},
	{'8', "8", true, {nullptr}},
	{'9', "9", true, {nullptr}},
	{'\0', nullptr, false, {nullptr}},
};

/* Delay targets */
static const struct fragment delay_targets[] = {
	{'b', " beacon #", false, {landmarks_terminal, nullptr}},
	{'*', " beacon #", false, {landmarks_terminal, nullptr}},
	{'\0', nullptr, false, {nullptr}}
};

/* The delay command */
static const struct fragment delay[] = {
	{'a', " at", false, {delay_targets, nullptr}},
	{'\0', nullptr, false, {nullptr}}
};

/* Absolute altitude numbers */
static const struct fragment altnum_absolute[] = {
	{'0', " 0000 feet", true, {nullptr}},
	{'1', " 1000 feet", true, {nullptr}},
	{'2', " 2000 feet", true, {nullptr}},
	{'3', " 3000 feet", true, {nullptr}},
	{'4', " 4000 feet", true, {nullptr}},
	{'5', " 5000 feet", true, {nullptr}},
	{'6', " 6000 feet", true, {nullptr}},
	{'7', " 7000 feet", true, {nullptr}},
	{'8', " 8000 feet", true, {nullptr}},
	{'9', " 9000 feet", true, {nullptr}},
	{'\0', nullptr, false, {nullptr}}
};

/* Relative altitude numbers */
static const struct fragment altnum_relative[] = {
	{'0', " 0000 ft", true, {nullptr}},
	{'1', " 1000 ft", true, {nullptr}},
	{'2', " 2000 ft", true, {nullptr}},
	{'3', " 3000 ft", true, {nullptr}},
	{'4', " 4000 ft", true, {nullptr}},
	{'5', " 5000 ft", true, {nullptr}},
	{'6', " 6000 ft", true, {nullptr}},
	{'7', " 7000 ft", true, {nullptr}},
	{'8', " 8000 ft", true, {nullptr}},
	{'9', " 9000 ft", true, {nullptr}},
	{'\0', nullptr, false, {nullptr}}
};

/* Climb and descend modifiers to Altitude */
static const struct fragment altmods[] = {
	{'c', " climb", false, {altnum_relative, nullptr}},
	{'+', " climb", false, {altnum_relative, nullptr}},
	{'d', " descend", false, {altnum_relative, nullptr}},
	{'-', " descend", false, {altnum_relative, nullptr}},
	{'\0', nullptr, false, {nullptr}},
};

/* Absolute directions */
static const struct fragment directions_absolute[] = {
	{'q', " to 315", true, {delay, nullptr}},
	{'w', " to 0", true, {delay, nullptr}},
	{'e', " to 45", true, {delay, nullptr}},
	{'a', " to 270", true, {delay, nullptr}},
	{'d', " to 90", true, {delay, nullptr}},
	{'z', " to 225", true, {delay, nullptr}},
	{'x', " to 180", true, {delay, nullptr}},
	{'c', " to 135", true, {delay, nullptr}},
	{'\0', nullptr, false, {nullptr}},
};

/* Relative directions */
static const struct fragment directions_relative[] = {
	{'q', " 315", true, {delay, nullptr}},
	{'w', " 0", true, {delay, nullptr}},
	{'e', " 45", true, {delay, nullptr}},
	/* NOTE! A is not present here because it means At instead of 270! */
	{'d', " 90", true, {delay, nullptr}},
	{'z', " 225", true, {delay, nullptr}},
	{'x', " 180", true, {delay, nullptr}},
	{'c', " 135", true, {delay, nullptr}},
	{'\0', nullptr, false, {nullptr}},
};

/* Single-character sharp turn angles */
static const struct fragment turns_sharp[] = {
	{'L', " left 90", true, {delay, nullptr}},
	{'R', " right 90", true, {delay, nullptr}},
	{'\0', nullptr, false, {nullptr}}
};

/* Moderate turns that accept optional angle parameters */
static const struct fragment turns_normal[] = {
	{'l', " left", true, {delay, directions_relative, nullptr}},
	{'r', " right", true, {delay, directions_relative, nullptr}},
	{'\0', nullptr, false, {nullptr}}
};

/* Landmark ID numbers (delayable) */
static const struct fragment landmarks_delayable[] = {
	{'0', "0", true, {delay, nullptr}},
	{'1', "1", true, {delay, nullptr}},
	{'2', "2", true, {delay, nullptr}},
	{'3', "3", true, {delay, nullptr}},
	{'4', "4", true, {delay, nullptr}},
	{'5', "5", true, {delay, nullptr}},
	{'6', "6", true, {delay, nullptr}},
	{'7', "7", true, {delay, nullptr}},
	{'8', "8", true, {delay, nullptr}},
	{'9', "9", true, {delay, nullptr}},
	{'\0', nullptr, false, {nullptr}},
};

/* Turns towards landmarks */
static const struct fragment turn_landmarks[] = {
	{'a', " airport #", false, {landmarks_delayable, nullptr}},
	{'b', " beacon #", false, {landmarks_delayable, nullptr}},
	{'e', " exit #", false, {landmarks_delayable, nullptr}},
	{'*', " beacon #", false, {landmarks_delayable, nullptr}},
	{'\0', nullptr, false, {nullptr}}
};

/* The word Towards */
static const struct fragment towards[] = {
	{'t', " towards", false, {turn_landmarks, nullptr}},
	{'\0', nullptr, false, {nullptr}}
};

/* The set of all commands */
static const struct fragment commands[] = {
	{'a', " altitude:", false, {altnum_absolute, altmods, nullptr}},
	{'m', " mark", true, {nullptr}},
	{'i', " ignore", true, {nullptr}},
	{'u', " unmark", true, {nullptr}},
	{'c', " circle", true, {nullptr}},
	{'t', " turn", false, {turns_sharp, turns_normal, directions_absolute, towards, nullptr}},
	{'\0', nullptr, false, {nullptr}}
};

/* The set of all plane letters */
static const struct fragment planes[] = {
	{'a', "a:", false, {commands, nullptr}},
	{'b', "b:", false, {commands, nullptr}},
	{'c', "c:", false, {commands, nullptr}},
	{'d', "d:", false, {commands, nullptr}},
	{'e', "e:", false, {commands, nullptr}},
	{'f', "f:", false, {commands, nullptr}},
	{'g', "g:", false, {commands, nullptr}},
	{'h', "h:", false, {commands, nullptr}},
	{'i', "i:", false, {commands, nullptr}},
	{'j', "j:", false, {commands, nullptr}},
	{'k', "k:", false, {commands, nullptr}},
	{'l', "l:", false, {commands, nullptr}},
	{'m', "m:", false, {commands, nullptr}},
	{'n', "n:", false, {commands, nullptr}},
	{'o', "o:", false, {commands, nullptr}},
	{'p', "p:", false, {commands, nullptr}},
	{'q', "q:", false, {commands, nullptr}},
	{'r', "r:", false, {commands, nullptr}},
	{'s', "s:", false, {commands, nullptr}},
	{'t', "t:", false, {commands, nullptr}},
	{'u', "u:", false, {commands, nullptr}},
	{'v', "v:", false, {commands, nullptr}},
	{'w', "w:", false, {commands, nullptr}},
	{'x', "x:", false, {commands, nullptr}},
	{'y', "y:", false, {commands, nullptr}},
	{'z', "z:", false, {commands, nullptr}},
	{'A', "A:", false, {commands, nullptr}},
	{'B', "B:", false, {commands, nullptr}},
	{'C', "C:", false, {commands, nullptr}},
	{'D', "D:", false, {commands, nullptr}},
	{'E', "E:", false, {commands, nullptr}},
	{'F', "F:", false, {commands, nullptr}},
	{'G', "G:", false, {commands, nullptr}},
	{'H', "H:", false, {commands, nullptr}},
	{'I', "I:", false, {commands, nullptr}},
	{'J', "J:", false, {commands, nullptr}},
	{'K', "K:", false, {commands, nullptr}},
	{'L', "L:", false, {commands, nullptr}},
	{'M', "M:", false, {commands, nullptr}},
	{'N', "N:", false, {commands, nullptr}},
	{'O', "O:", false, {commands, nullptr}},
	{'P', "P:", false, {commands, nullptr}},
	{'Q', "Q:", false, {commands, nullptr}},
	{'R', "R:", false, {commands, nullptr}},
	{'S', "S:", false, {commands, nullptr}},
	{'T', "T:", false, {commands, nullptr}},
	{'U', "U:", false, {commands, nullptr}},
	{'V', "V:", false, {commands, nullptr}},
	{'W', "W:", false, {commands, nullptr}},
	{'X', "X:", false, {commands, nullptr}},
	{'Y', "Y:", false, {commands, nullptr}},
	{'Z', "Z:", false, {commands, nullptr}},
	{'\0', nullptr, false, {nullptr}}
};

/* The root metalist. */
const struct fragment *root[] = {
	planes,
	nullptr
};



static const struct fragment *find_fragment(const struct fragment * const *fraglist, char input) {
	for (unsigned int i = 0; i < MAX_FOLLOWING_FRAGMENT_LISTS && fraglist[i]; i++)
		for (unsigned int j = 0; fraglist[i][j].input; j++)
			if (fraglist[i][j].input == input)
				return &fraglist[i][j];
	return nullptr;
}



bool parse_command(const char *readptr, char *writeptr, size_t writelen, bool *terminal) {
	/* An empty buffer is considered an error. */
	if (!writelen)
		return false;

	/* An empty string is actually considered acceptable and terminal. */
	*writeptr = '\0';
	if (readptr[0] == '\0') {
		*terminal = true;
		return true;
	}

	/* Check for a chat message. */
	if (readptr[0] == '/') {
		if (strlen(readptr + 1) + 6 /* "chat: " */ + 1 > writelen)
			return false;
		strcpy(writeptr, "chat: ");
		strcat(writeptr, readptr + 1);
		*terminal = readptr[1] != '\0';
		return true;
	}

	/* Iterate the characters in the input. */
	const struct fragment * const *fraglist = root;
	const struct fragment *fragptr = nullptr;
	while (*readptr) {
		/* Given the current set of acceptable fragment lists, try to find a fragment matching the input char. */
		fragptr = find_fragment(fraglist, *readptr++);
		/* If we didn't find any such fragment, give up. */
		if (!fragptr)
			return false;
		/* If there's not enough buffer space left, give up. */
		if (strlen(writeptr) + strlen(fragptr->output) + 1 > writelen)
			return false;
		/* Append the found fragment's description to the output buffer. */
		strcat(writeptr, fragptr->output);
		/* Look for the next character in the followers list of the found fragment. */
		fraglist = fragptr->followers;
	}

	/* We got to the end of the input, which means we're successful. */
	*terminal = fragptr->terminal;
	return true;
}

