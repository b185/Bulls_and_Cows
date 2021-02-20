# Bulls_and_Cows
Introduction to Operating Systems course at Tel-Aviv University Final Project - "Bulls&Cows"


This is a C programming language application compiled in Windows environment with the Microsoft Visual Studios 2019 x86 compiler. 
The application is divided into two seperate programs: First is the "Server" program, and the second is the "Client" program. Every program is meant to run in a seperate process (different CMD console).

The "Server" process is meant to act as a Waiting&Gaming room for up to 3 "Client" processes, and each of these "Client" represents a Player(User) of the game and relays the User's commands. Which means the "Server" listens for incoming "Client" processes connections, connect a pair of two "Client"s between one another and conducts a "Bulls & Cows" game between the two. 

The "Server" and the "Client" establishes a TCP connection using the Winsock library, and utilizes a multi-threaded synchronized operation with shared resources and the WinAPI library.





The "Bulls & Cows" game is conducted in the following manner: 

When two plyers are matched, initially, every player chooses a single number comprised of 4 unique(different from one another) digits (0-9). 

After this initialization, the game proceeds to "Rounds" phase. 
At every round, every player guesses the other player's number. If a guess' digit at some location equals a digit in a different location in the initial number, then this counts a single "Cow". If a guess' digit at some location equals a digit in the same location in the initial number, then this counts a single "Bull". 

At the end of every round, if neither a win nor a tie occured, then both players are shown the number of cows and bulls their guess yielded, and also the opponents guess of their(self) initial number. 
If one player completely guesses the other player's initial number, then the game comes to an end when the player guessing correctly wins. 
If both players guessed correctlly the other player initial number at the same round, then the game comes to an end with a "Draw" result.



Notes:

  1) The "Server" should be activated first, and "Client"s should be activated afterwards(up to 3 "Client"s at a time).
  2) Legal and correct inputs should be inserted at every phase of the program. Multiple choices can be inserted at different menus, and these choices will be enlisted clearly.




Testing:

The communication between the different processes is set by default to be executed from the same computer. One can perform testing from different computers by first altering the "Server"'s listening socket's definitions.

The input to the "Server" program is the port number on which the process will listen for incoming "Client"s connections: input in Windows Command Prompt opened from "Debug" folder - "Server.exe <listening socket's port number ranging from 1024-65536>"

The inputs to the "Client" program are (in the following order) the "Server"'s computer IP address, the "Server"'s computer listening port number (same as the "Server"'s input) and the name (at most 20 characters) of the Player\user. According to the server's default listening socket's settings, insert the IP address 127.0.0.1 to connect the "Server" and "Client"s within a single computer: input in Windows Command Prompt opened from "Debug" folder - "Client.exe <Server's IP address> <listening socket's port number ranging from 1024-65536> <Player's name - up to 20 characters>"

The outputs of both the "Server" and the "Client" will be a "success" message if the communication prcedure hasn't met fatal errors.
