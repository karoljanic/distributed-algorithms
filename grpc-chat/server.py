import grpc
from concurrent import futures
import queue
import threading
import chat_pb2
import chat_pb2_grpc


class ChatServer(chat_pb2_grpc.ChatServerServicer):
    def __init__(self):
        self.clients_ = []
        self.clients_lock_ = threading.Lock()

    def ChatStream(self, request, context):
        q = queue.Queue()
        with self.clients_lock_:
            self.clients_.append(q)
        
        print(f"New client connected. Total number of clients: {len(self.clients_)}")

        try:
            while True:
                try:
                    note = q.get(timeout=10)
                    yield note
                except queue.Empty:
                    if not context.is_active():
                        break
        finally:
            with self.clients_lock_:
                if q in self.clients_:
                    self.clients_.remove(q)
                print(f"Client disconnected. Total number of clients: {len(self.clients_)}")

    def SendNote(self, request, context):
        print(f"Sending messages to {request.name}")
        with self.clients_lock_:
            clients = list(self.clients_)

        for q in clients:
            try:
                q.put(request)
            except Exception:
                pass

        return chat_pb2.Empty()

def serve():
    server = grpc.server(futures.ThreadPoolExecutor(max_workers=10))
    chat_pb2_grpc.add_ChatServerServicer_to_server(ChatServer(), server)

    server.add_insecure_port('[::]:8088')
    server.start()

    print("Server running on port 8088...")

    try:
        server.wait_for_termination()
    except KeyboardInterrupt:
        print("Shutting down server...")
        server.stop(5)


if __name__ == '__main__':
    serve()
