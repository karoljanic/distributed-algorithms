import grpc
import threading
import os
import signal
import chat_pb2
import chat_pb2_grpc


class ChatClient:
    def __init__(self, name, address):
        self.name_ = name
        self.address_ = address
        self.channel_ = grpc.insecure_channel(address)
        self.stub_ = chat_pb2_grpc.ChatServerStub(self.channel_)
        self.stop_event = threading.Event()

    def listen_for_messages(self):
        try:
            responses = self.stub_.ChatStream(chat_pb2.Empty())
            for note in responses:
                if self.stop_event.is_set():
                    break
                if note.name == self.name_:
                    continue
                print(f"{note.name}: {note.message}")
        except grpc.RpcError as e:
            if e.code() == grpc.StatusCode.UNAVAILABLE:
                print("Server is unavailable")
            else:
                msg = e.details() or str(e)
                print(f"Connection lost: {msg}")
            self.stop_event.set()
            
            try:
                self.channel_.close()
            except Exception:
                pass

            try:
                os.kill(os.getpid(), signal.SIGINT)
            except Exception:
                pass
            return

    def run(self):
        listener_thread = threading.Thread(target=self.listen_for_messages)
        listener_thread.daemon = True
        listener_thread.start()
        try:
            while True:
                try:
                    message = input(">> ")
                except KeyboardInterrupt:
                    break

                if not message.strip():
                    continue

                if message.lower() == 'exit':
                    break

                try:
                    self.stub_.SendNote(chat_pb2.Note(name=self.name_, message=message))
                except grpc.RpcError as e:
                    print(f"Failed to send message: {e}")
                    break
        finally:
            self.stop_event.set()
            try:
                self.channel_.close()
            except Exception:
                pass
            listener_thread.join(timeout=5)


if __name__ == '__main__':
    name = input("Enter your name: ")
    client = ChatClient(name, 'localhost:8088')
    client.run()
