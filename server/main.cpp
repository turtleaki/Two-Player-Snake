#include <stdlib.h>
#include <iostream>
#include <string>
#include <sstream>
#include <time.h>
#include "websocket.h"
#include <random>
#include <chrono>


using namespace std;

webSocket server;

int SNAKE_LENGTH = 5;


struct Snake{
	std::string ID;
	std::vector<std::vector<int>> snake_array;
	int score;

};

struct Message{
	int clientID;
	std::string message;
	int latencyVal;
};

//Map client ID to snake.
std::map<int, Snake > clientSnakes;
//Queue implementing artificial latency for sending messages.
std::map<std::chrono::steady_clock::time_point, std::vector<Message>> SendQueue;
//Queue implementing artificial latency for receiving messages.
std::map<std::chrono::steady_clock::time_point, std::vector<Message>> ReceiveQueue;
int serverDelayStart;


//Randomizes latency with a base value plus a random value
//Example: Base = 100, modifier = 50...latencyValue will be between 101-150
int randomizeLatency(int modifier, int base){
	int latencyValue = 0;
	latencyValue = (std::rand() % (modifier+1)) + base;
	return latencyValue;
}

//Creates and initializes a msg struct for the Queues
Message createMessage(int cID, std::string msg, int modifier = 10, int base = 100){
	Message msgStruct;
	msgStruct.clientID = cID;
	msgStruct.message = msg;
	msgStruct.latencyVal = randomizeLatency(modifier, base);
	return msgStruct;
}

//Creates a snake at (0,y_pos)
std::vector<std::vector<int>> createSnake(int y_pos){
	std::vector<std::vector<int>> snake;
	for (int i = SNAKE_LENGTH - 1; i >= 0; i--){
		snake.push_back({ i, y_pos });
	}
	return snake;
}

//Randomly creates coordinates for food
std::vector<int> createFood(){
	std::vector<int> food;
	food.push_back(std::rand() % 50);
	food.push_back(std::rand() % 35);
	return food;
}

//Converts snake values into a string that we can send to the client
std::string snakeToString(std::vector<std::vector<int>> snake_array){
	std::string ret_str = "";
	for (int i = 0; i < snake_array.size(); i++){
		ret_str += std::to_string(snake_array[i][0]) + "," + std::to_string(snake_array[i][1]);
		if (i < snake_array.size() - 1){
			ret_str += "|";
		}
	}
	return ret_str;
}

//Converts string values into a vector of ints, representing coords
std::vector<std::vector<int>> stringToSnake(std::string snake_array){
	std::vector<std::vector<int>> ret_snake;
	std::vector<std::string> temp;
	std::string to_add = "";
	for (int i = 0; i < snake_array.size(); i++){
		to_add += snake_array[i];
		if (snake_array[i] == *"|" || i == snake_array.size() - 1){
			std::string snake_x = "";
			std::string snake_y = "";
			for (int k = 0; k < to_add.size(); k++){
				snake_x += to_add[k];
				if (to_add[k] == *","){
					for (int j = k + 1; j < to_add.size(); j++){
						snake_y += to_add[j];
					}
					break;
				}
			}
			std::vector<int> temp = { atoi(snake_x.c_str()), atoi(snake_y.c_str()) };
			ret_snake.push_back(temp);
			to_add = "";
		}
	}

	return ret_snake;
}

//Checks for collisions
bool check_collision(int x, int y, std::vector<std::vector<int>> snake_array){
	for (int i = 0; i < snake_array.size(); i++){
		if (snake_array[i][0] == x && snake_array[i][1] == y){
			return true;
		}
	}
	return false;
}

//Checks for winners
std::string check_winner(){
	vector<int> clientIDs = server.getClientIDs();
	std::string winner;
	if (clientSnakes[clientIDs[0]].score > clientSnakes[clientIDs[1]].score){
		winner = "Winner: " + clientSnakes[clientIDs[0]].ID;
	}
	else if (clientSnakes[clientIDs[0]].score < clientSnakes[clientIDs[1]].score){
		winner = "Winner: " + clientSnakes[clientIDs[1]].ID;
	}
	else if (clientSnakes[clientIDs[0]].score == clientSnakes[clientIDs[1]].score){
		winner = "It's a tie";
	}
	return winner;

}

//Provides info for current start of snake game and sends the notification to start the game to the client.
void gameStartState(){
	vector<int> clientIDs = server.getClientIDs();
	std::vector<int> food = createFood();

	Snake player1 = clientSnakes[clientIDs[0]];
	Snake player2 = clientSnakes[clientIDs[1]];
	player1.snake_array = createSnake(0);
	player2.snake_array = createSnake(34);
	player1.score = 0;
	player2.score = 0;
	clientSnakes[clientIDs[0]].score = 0;
	clientSnakes[clientIDs[1]].score = 0;
	clientSnakes[clientIDs[0]].snake_array = player1.snake_array;
	clientSnakes[clientIDs[1]].snake_array = player2.snake_array;

	std::vector<Snake> snakes = { player1, player2 };

	SendQueue[std::chrono::steady_clock::now()].push_back(createMessage(clientIDs[0], "st;" + snakeToString(player1.snake_array) + "; " + snakeToString(player2.snake_array) + "; "
		+ std::to_string(player1.score) + "," + std::to_string(player2.score) + ";" + std::to_string(food[0]) + "," + std::to_string(food[1])));

	SendQueue[std::chrono::steady_clock::now()].push_back(createMessage(clientIDs[1], "st;" + snakeToString(player2.snake_array) + "; " + snakeToString(player1.snake_array) + "; "
		+ std::to_string(player2.score) + "," + std::to_string(player1.score) + ";" + std::to_string(food[0]) + "," + std::to_string(food[1])));
}

/* called when a client connects */
void openHandler(int clientID){
	ostringstream os;
	vector<int> clientIDs = server.getClientIDs();

	std::cout << "in openhandler " << clientIDs.size() << std::endl;
	if (clientIDs.size() > 3){
		server.wsClose(clientID);
		cout << "Connection rejected: Only two connection allowed at a time.";
	}
	else if (clientIDs.size() == 2)
	{
		std::cout << "size is 2" << std::endl;
		os << "Stranger " << clientID << " has joined.";

		for (int i = 0; i < clientIDs.size(); i++){
			clientSnakes[clientIDs[i]].ID = clientID;
			server.wsSend(clientIDs[i], "Welcome!");
			//SendQueue[std::chrono::steady_clock::now()].push_back(createMessage(clientIDs[i], "Welcome!"));
		}

		gameStartState();

	}
}

/* called when a client disconnects */
void closeHandler(int clientID){
	ostringstream os;
	os << "Stranger " << clientID << " has leaved.";

	vector<int> clientIDs = server.getClientIDs();
	for (int i = 0; i < clientIDs.size(); i++){
		server.wsSend(clientIDs[i], "Disconnected");
		server.wsClose(clientIDs[i]);
	}
	ReceiveQueue.clear();
	SendQueue.clear();

}

//Handles messages, sending them into the ReceiveQueue. When they leave the Receive Queue, they get sent to the messageHandler
void latencyMessageHandler(int clientID, string message){
	ReceiveQueue[std::chrono::steady_clock::now()].push_back(createMessage(clientID, message));
}

/* called when a client sends a message to the server */
void messageHandler(int clientID, string message){
	std::vector<int> clientIDs = server.getClientIDs();
	if (clientSnakes.find(clientID) != clientSnakes.end()){
		std::string toSend;

		if (message == "GameOver"){
			std::string winner = check_winner();
			for (int i = 0; i < clientIDs.size(); i++){
				//server.wsSend(clientIDs[i], "winner;" + winner);
				SendQueue[std::chrono::steady_clock::now()].push_back(createMessage(clientIDs[i], "winner;" + winner));
			}
			gameStartState();
		}
		else if (message.substr(0, 2) == "sn"){
			clientSnakes[clientID].snake_array = stringToSnake(message.substr(2));
			Snake player1 = clientSnakes[clientID];
			Snake player2;
			for (int i = 0; i < clientIDs.size(); i++){
				if (clientID != clientIDs[i]){
					player2 = clientSnakes[clientIDs[i]];
				}
			}
			if (check_collision(player1.snake_array[0][0], player1.snake_array[0][1], player2.snake_array)){
				gameStartState();
			}

			for (int i = 0; i < clientIDs.size(); i++){
				if (clientIDs[i] != clientID){
					SendQueue[std::chrono::steady_clock::now()].push_back(createMessage(clientIDs[i], "sn;" + message.substr(2)));
				}
			}
		}
		else if (message == "Food"){
			clientSnakes[clientID].score++;
			std::vector<int> food = createFood();
			for (int i = 0; i < clientIDs.size(); i++){
				if (clientIDs[i] != clientID){
					SendQueue[std::chrono::steady_clock::now()].push_back(createMessage(clientIDs[i], "fs;" + std::to_string(food[0]) + "," + std::to_string(food[1]) + ";" +
						std::to_string(clientSnakes[clientIDs[i]].score) + "," + std::to_string(clientSnakes[clientID].score)));
				}
				else if (clientIDs[i] == clientID){
					std::string otherscore;
					for (auto key : clientIDs){
						if (key != clientID){
							otherscore = std::to_string(clientSnakes[key].score);
						}
					}
					SendQueue[std::chrono::steady_clock::now()].push_back(createMessage(clientIDs[i], "fs;" + std::to_string(food[0]) + "," + std::to_string(food[1]) + ";" +
						std::to_string(clientSnakes[clientIDs[i]].score) + "," + otherscore));
				}
			}

		}
		else if (message.substr(0, 2) == "id"){
			clientSnakes[clientID].ID = message.substr(2);

			for (int i = 0; i < clientIDs.size(); i++){
				if (clientIDs[i] != clientID){
					SendQueue[std::chrono::steady_clock::now()].push_back(createMessage(clientIDs[i], "id;" + clientSnakes[clientID].ID));
				}
			}
		}
		else if (message == "Latency"){
			std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
			for (int i = 0; i < clientIDs.size(); i++){
				SendQueue[std::chrono::steady_clock::now()].push_back(createMessage(clientIDs[i], "latency"));
			}
			serverDelayStart = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
		}

	}

}


//Checks for messages in the Queues. Checks their time stamps and releases them when they have been delayed by their respective latency value.
/* called once per select() loop */
void periodicHandler(){
	if (clientSnakes.size() == 2){
		std::chrono::steady_clock::time_point other = std::chrono::steady_clock::now();
		if (!ReceiveQueue.empty()){
			//use iterators so that we can edit queue during iteration.
			for (auto msg = ReceiveQueue.begin(); msg != ReceiveQueue.end();){
				for (auto vect = msg->second.begin(); vect != msg->second.end();){
					if (std::chrono::duration_cast<std::chrono::milliseconds>(other - msg->first).count() > vect->latencyVal){
						messageHandler(vect->clientID, vect->message);
						//removes message from queue
						vect = msg->second.erase(vect);
					}
					else{
						++vect;
					}
				}
				if (msg->second.empty()){
					msg = ReceiveQueue.erase(msg);
				}
				else{
					++msg;
				}
			}
		}
		std::chrono::steady_clock::time_point other1 = std::chrono::steady_clock::now();
		if (!SendQueue.empty()){
			for (auto key = SendQueue.begin(); key != SendQueue.end();){
				for (auto vec = key->second.begin(); vec != key->second.end();){
					if (std::chrono::duration_cast<std::chrono::milliseconds>(other1 - key->first).count() > vec->latencyVal){
						if (vec->message == "latency"){
							vec->message = "latency;" + std::to_string(serverDelayStart);
						}
						server.wsSend(vec->clientID, vec->message);
						vec = key->second.erase(vec);
					}
					else{
						++vec;
					}
				}
				if (key->second.empty()){
					key = SendQueue.erase(key);
				}
				else{
					++key;
				}
			}
		}

	}

}


int main(int argc, char *argv[]){
	int port;

	cout << "Please set server port: ";
	cin >> port;

	/* set event handler */

	server.setOpenHandler(openHandler);
	server.setCloseHandler(closeHandler);
	server.setMessageHandler(latencyMessageHandler);
	server.setPeriodicHandler(periodicHandler);

	/* start the chatroom server, listen to ip '127.0.0.1' and port '8000' */
	server.startServer(port);


	return 1;
}
