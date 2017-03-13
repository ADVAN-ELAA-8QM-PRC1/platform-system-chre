/*
 * Copyright (C) 2017 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <poll.h>

#include <atomic>
#include <functional>
#include <map>
#include <mutex>

#include <android-base/macros.h>
#include <cutils/sockets.h>

namespace android {
namespace chre {

class SocketServer {
 public:
  SocketServer();

  /**
   * Defines the function signature of the callback given to run() which
   * receives message data sent in by a client.
   *
   * @param clientId A unique identifier for the client that sent this request
   *        (assigned locally)
   * @param data Pointer to buffer containing the raw message data
   * @param len Number of bytes of data received
   */
  typedef std::function<void(uint16_t clientId, const void *data, size_t len)>
      ClientMessageCallback;

  /**
   * Opens the socket, and runs the receive loop until an error is encountered,
   * or SIGINT/SIGTERM is received.
   *
   * @param socketName Android socket name to use when listening
   * @param allowSocketCreation If true, allow creation of the socket rather
   *        than strictly inheriting it from init (used primarily for
   *        development purposes)
   * @param clientMessageCallback Callback to be invoked when a message is
   *        received from a client
   */
  void run(const char *socketName, bool allowSocketCreation,
           ClientMessageCallback clientMessageCallback);

  /**
   * Delivers data to all connected clients. This method is thread-safe.
   *
   * @param data Pointer to buffer containing message data
   * @param length Number of bytes of data to send
   */
  void sendToAllClients(const void *data, size_t length);

 private:
  DISALLOW_COPY_AND_ASSIGN(SocketServer);

  static constexpr int kMaxPendingConnectionRequests = 4;
  static constexpr size_t kMaxActiveClients = 4;
  static constexpr size_t kMaxPacketSize = 4096;

  int mSockFd = INVALID_SOCKET;
  uint16_t mNextClientId = 1;
  struct pollfd mPollFds[1 + kMaxActiveClients] = {};

  struct ClientData {
    uint16_t clientId;
  };

  // Maps from socket FD to ClientData
  std::map<int, ClientData> mClients;

  // Ensures that mClients can be safely iterated over from other threads
  // without worrying about potential modification from the RX thread
  std::mutex mClientsMutex;

  ClientMessageCallback mClientMessageCallback;

  void acceptClientConnection();
  void disconnectClient(int clientSocket);
  void handleClientData(int clientSocket);
  void serviceSocket();

  static std::atomic<bool> sSignalReceived;
  static void signalHandler(int signal);
};

}  // namespace chre
}  // namespace android
