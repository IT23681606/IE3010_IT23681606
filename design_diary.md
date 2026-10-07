
## Final Testing Evidence

The NetMessenger application was tested using the personalised server and client programs. The server was verified to listen on TCP port 7606 using the ss command. Client registration was tested using the REGISTER command, and the LIST command was used to verify registered users and their NID values. The client-server connection was also tested successfully using the localhost address.

Additional testing included command validation and handling of invalid or unsupported requests. The testing process helped identify and improve command validation and error handling in the client and server implementation.

The personalised storage directory was also verified and a test file was successfully created in the required storage path. These tests provided evidence that the main client-server communication and file-storage components were functioning as expected.
