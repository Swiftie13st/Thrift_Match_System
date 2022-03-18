// This autogenerated skeleton file illustrates how to build a server.
// You should copy it to another filename to avoid overwriting it.

#include "match_server/Match.h"
#include "save_client/Save.h"

#include <thrift/concurrency/ThreadManager.h>
#include <thrift/concurrency/ThreadFactory.h>
#include <thrift/protocol/TBinaryProtocol.h>
#include <thrift/server/TThreadedServer.h>
#include <thrift/transport/TServerSocket.h>
#include <thrift/transport/TBufferTransports.h>
#include <thrift/transport/TSocket.h>
#include <thrift/transport/TTransportUtils.h>
#include <thrift/TToString.h>

#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <vector>
#include <unistd.h>

using namespace ::apache::thrift;
using namespace ::apache::thrift::protocol;
using namespace ::apache::thrift::transport;
using namespace ::apache::thrift::server;

using namespace ::match_service;
using namespace ::save_service;

using namespace std;

struct Task
{
    User user;
    string type;
};

struct MessageQueue
{
    queue<Task> q;
    mutex m;
    condition_variable cv;
}message_queue;


class Pool
{
    public:
        void match()
        {
            for(uint32_t i = 0; i < wt.size(); i ++)
            {
                wt[i] ++;
            }


            while(users.size() > 1)
            {
                //sort(users.begin(), users.end(), [&](User &a, User b){ return a.score < b.score; }); 按分数排序
                
                bool flag = true; //防止死循环，当没有匹配上时跳出循环
                for(uint32_t i = 0; i < users.size(); i ++)
                {
                    for(uint32_t j = i + 1; j < users.size(); j ++)
                    {
                        auto a = users[i], b = users[j];
                        if(check_match(i, j))
                        {
                            users.erase(users.begin() + j);
                            users.erase(users.begin() + i);
                            wt.erase(wt.begin() + j);
                            wt.erase(wt.begin() + i);
                            save_result(a.id, b.id);
                            flag = false;
                            break;
                        }
                    }
                }
                if(flag) { break; }
            }

        }

        bool check_match(uint32_t i, uint32_t j)
        {
            auto a = users[i], b = users[j];
            int dt = abs(a.score - b.score); // 分差
            int a_max_dif = wt[i] * 50; //每秒多50
            int b_max_dif = wt[j] * 50;

            return dt <= a_max_dif && dt <= b_max_dif;
        }

        void save_result(int a, int b)
        {
            cout << "Match Result: " << a << ' ' << b << endl;
            std::shared_ptr<TTransport> socket(new TSocket("123.57.47.211", 9090));
            std::shared_ptr<TTransport> transport(new TBufferedTransport(socket));
            std::shared_ptr<TProtocol> protocol(new TBinaryProtocol(transport));
            SaveClient client(protocol);

            try {
                transport->open();

                int res = client.save_data("acs_3447", "eb4853ef", a, b);
                if(res == 0)
                {
                    cout << "Success" << endl;
                }
                else if(res == 1)
                {
                    cout << "Error" << endl;
                }


                transport->close();
            } catch (TException& tx) {
                cout << "ERROR: " << tx.what() << endl;
            }
        }

        void add(User user)
        {
            users.push_back(user);
            wt.push_back(0);
        }

        void remove(User user)
        {
            for(uint32_t i = 0; i < users.size(); i ++)
            {
                if(users[i].id == user.id)
                {
                    users.erase(users.begin() + i);
                    wt.erase(wt.begin() + i);
                    break;
                }
            }
        }

    private:
        vector<User> users;
        vector<int> wt; //Waitting time等待时间 匹配轮次
}pool;


class MatchHandler : virtual public MatchIf {
    public:
        MatchHandler() {
            // Your initialization goes here
            // vector<int> wt; //Waitting time等待时间
        }

        /**
         * user: 添加的用户信息
         * info: 附加信息
         * 在匹配池中添加一个名用户
         * 
         * @param user
         * @param info
         */
        int32_t add_user(const User& user, const std::string& info) {
            // Your implementation goes here
            printf("add_user\n");

            unique_lock<mutex> lck(message_queue.m);
            message_queue.q.push({user, "add"});
            message_queue.cv.notify_all();
            return 0;
        }

        /**
         * user: 删除的用户信息
         * info: 附加信息
         * 从匹配池中删除一名用户
         * 
         * @param user
         * @param info
         */
        int32_t remove_user(const User& user, const std::string& info) {
            // Your implementation goes here
            printf("remove_user\n");

            unique_lock<mutex> lck(message_queue.m);
            message_queue.q.push({user, "remove"});
            message_queue.cv.notify_all();

            return 0;
        }

};



void consume_task()
{
    while(true)
    {
        unique_lock<mutex> lck(message_queue.m);
        if(message_queue.q.empty())
        {
            // message_queue.cv.wait(lck);
            lck.unlock();
            pool.match();
            sleep(1);   // 每秒匹配一次
        }
        else
        {
            auto task = message_queue.q.front();
            message_queue.q.pop();
            lck.unlock();
            if(task.type == "add")
            {
                pool.add(task.user);
            }
            else if(task.type == "remove")
            {
                pool.remove(task.user);
            }



        }
    }
}
class MatchCloneFactory : virtual public MatchIfFactory {
    public:
        ~MatchCloneFactory() override = default;
        MatchIf* getHandler(const ::apache::thrift::TConnectionInfo& connInfo) override
        {
            std::shared_ptr<TSocket> sock = std::dynamic_pointer_cast<TSocket>(connInfo.transport);
            /*cout << "Incoming connection\n";
            cout << "\tSocketInfo: "  << sock->getSocketInfo() << "\n";
            cout << "\tPeerHost: "    << sock->getPeerHost() << "\n";
            cout << "\tPeerAddress: " << sock->getPeerAddress() << "\n";
            cout << "\tPeerPort: "    << sock->getPeerPort() << "\n";*/
            return new MatchHandler;
        }
        void releaseHandler(MatchIf* handler) override {
            delete handler;
        }
};


int main(int argc, char **argv) {
    TThreadedServer server(
            std::make_shared<MatchProcessorFactory>(std::make_shared<MatchCloneFactory>()),
            std::make_shared<TServerSocket>(9090), //port
            std::make_shared<TBufferedTransportFactory>(),
            std::make_shared<TBinaryProtocolFactory>());
    // TSimpleServer server(processor, serverTransport, transportFactory, protocolFactory);

    cout << "Start Match Server" << endl;

    thread matching_thread(consume_task);

    server.serve();
    return 0;
}

