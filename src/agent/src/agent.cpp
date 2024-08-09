#include <agent.hpp>

#include <shared.hpp>

#include <string>
#include <thread>

namespace
{
    template<MessageType T>
    auto getMessagesFromQueue(MultiTypeQueue& multiTypeQueue)
    {
        return [&multiTypeQueue]() -> boost::asio::awaitable<std::string>
        {
            std::cout << "Getting messages from queue\n";
            const auto message = co_await multiTypeQueue.getNextAwaitable(T);
            co_return message.data.dump();
        };
    }

    template<MessageType T>
    auto popMessagesFromQueue(MultiTypeQueue& multiTypeQueue)
    {
        return [&multiTypeQueue](const std::string& response) -> void
        {
            std::cout << "Response: " << response << '\n';
            std::cout << "Popping messages from queue\n";
            multiTypeQueue.pop(T);
        };
    }
} // namespace

Agent::Agent()
    : m_communicator(m_agentInfo.GetUUID(),
                     [this](std::string table, std::string key) -> std::string
                     { return m_configurationParser.GetConfig<std::string>(table, key); })
{
    m_taskManager.Start(std::thread::hardware_concurrency());
}

Agent::~Agent()
{
    m_taskManager.Stop();
}

void Agent::Run()
{
    m_taskManager.EnqueueTask(m_communicator.WaitForTokenExpirationAndAuthenticate());
    m_taskManager.EnqueueTask(m_communicator.GetCommandsFromManager(getMessagesFromQueue<COMMAND>(m_messageQueue),
                                                                    popMessagesFromQueue<COMMAND>(m_messageQueue)));
    m_taskManager.EnqueueTask(m_communicator.StatefulMessageProcessingTask(
        getMessagesFromQueue<STATEFUL>(m_messageQueue), popMessagesFromQueue<STATEFUL>(m_messageQueue)));
    m_taskManager.EnqueueTask(m_communicator.StatelessMessageProcessingTask(
        getMessagesFromQueue<STATELESS>(m_messageQueue), popMessagesFromQueue<STATELESS>(m_messageQueue)));

    m_signalHandler.WaitForSignal();
}
