# Hybrid Instant Messaging Application   

## Overview  
This project is a hybrid instant messaging application that combines **Client-Server** and **Peer-to-Peer (P2P)** communication models. The application enables users to engage in **one-to-one chats** or **group conversations**, offering flexibility and efficient resource utilization.  

### Key Features  
- **One-to-One Chat**:  
  Users can start private chats. Device information (ID and port) is fetched from the server, and a direct P2P connection is established, offloading communication from the server.  

- **Group Chat**:  
  Users in a one-to-one chat can seamlessly add others to form a group. Each participant creates direct P2P sockets with every other participant, ensuring decentralized communication.  

- **Offline Messaging**:  
  Messages sent to offline or busy users are stored on the server in a dedicated "pending messages" directory. These are delivered when the recipient becomes available.  

- **Crash Handling**:  
  Devices detect crashes based on communication errors and update their connections accordingly to maintain an uninterrupted experience for active users.  

- **Chat History**:  
  Maintains message history for one-to-one chats for reference and continuity.  

- **File Sharing**:  
  Files can be shared using the `/s` command. Received files are saved locally for easy access.  

- **User Directory and Contact List**:  
  A built-in directory system stores usernames and contact details, while a dynamic `chatDevices` variable tracks active chats.  

- **Server Recovery**:  
  Server states are saved upon disconnection in a `server_info.txt` file for smooth recovery during restarts.  

- **Timestamps**:  
  User activity is tracked with timestamps formatted as `HH:MM:SS`, dynamically updated with a `tempo_attuale()` function.  

## 🛠️ Technologies and Tools  
- **Socket Programming**: Ensures real-time communication.  
- **Hybrid Architecture**: Combines the benefits of Client-Server and Peer-to-Peer models.  
- **File Management**: For offline message storage, contact directory, and server persistence.  

## 🚀 Getting Started  
1. Clone this repository.  
2. Start the server to manage user data and offline messages.  
3. Launch the client application to initiate one-to-one or group chats, or share files using the `/s` command.  

### File Sharing Example  
To test file sharing:  
- Share a file named `fileprova.txt`.  
- Check the received file in `recv.txt`.  

**Developer**: Francesco Moretti  
**University**: University of Pisa, Computer Networks (2022/2023) 
