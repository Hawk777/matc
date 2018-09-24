This is a program for use with the BSD-Games package’s `atc` game. It allows
multiple users to send commands to the game at once, providing each user with
their own command entry area and delivering their keystrokes to the game only
once a full command was entered. It also allows pausing the game, which the
game originally did not support. You will generally want to run `atcd` inside
`tmux` or `screen` so both players can see the game.

matc is © Christopher Head and is released under the GNU General Public License
version 3.
