# csc209-a4

A battle server that runs on a server hosted by my university, the University of Toronto.  The assignment details are listed below

#Login
When a new client arrives, add them to the end of the active clients list, and ask for and store their name. Tell the new client that they are awaiting an opponent; tell everyone else that someone new has entered the arena. Scan the connected clients; if a suitable client is available, start a match with the newly-connected client.

#Matching
A client should never wait for a match if a suitable opponent exists. Consider clients A and B:

If A or B is currently in a match, then A and B cannot be matched
If A last battled B, and B last battled A, then A and B cannot be matched
Otherwise, they can be matched
In particular, new matches are possible when a new client logs-in and when an existing match terminates by completing normally or due to a client dropping. Suitable partners should be searched starting from the beginning of the client list. Once a match finishes, both partners should be moved to the end of the client list.

#Combat
When two players are matched, they combat until one loses all their hitpoints or one of the players drops. Players take turns attacking. Call the currently-attacking player the active player. Only the active player can say something; any text sent by the inactive player should be discarded. Any invalid commands sent by the active player should also be discarded. (This includes hitting p when no powermoves are available.)

You are encouraged to experiment with these parameters, but here is what I used:

Each player starts a match with between 20 and 30 hitpoints. (Note that hitpoints and powermoves are reset to random values on the start of a new match, independent of what the values may have been following their previous match.)
Each player starts a match with between one and three powermoves.
Damage from a regular attack is 2-6 hitpoints.
Powermoves have a 50% chance of missing. If they hit, then they cause three times the damage of a regular attack.
The active player should be sent a menu of valid commands before each move. The p command should not be printed (or accepted) if the player has no powermoves remaining.

To generate random numbers, use srand once on program entry to seed the random number generator, and then keep calling rand to get random numbers. King 10.2 contains an example of doing this.

#Dropping
When a client drops, advertise to everyone else that the client is gone. If the dropping client was engaged in a match, their opponent should be notified as the winner and told that they are awaiting a new opponent. The match involving the dropping client should be removed.

© 2015 Karen Reid
