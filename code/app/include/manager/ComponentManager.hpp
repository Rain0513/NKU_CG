#pragma once
#ifndef __NR_COMPONENT_MANAGER_HPP__
#define __NR_COMPONENT_MANAGER_HPP__

#include <Windows.h>
#include <vector>
#include <chrono>
#include <thread>
#include <atomic>

#include "component/RenderComponent.hpp"
#include "server/Server.hpp"

namespace NRenderer
{
    using namespace std;
    class DLL_EXPORT ComponentManager
    {
    public:
        enum class State
        {
            IDLING,
            READY,
            RUNNING,
            FINISH
        };
    private:
        std::atomic<State> state;
        vector<HMODULE> loadedDlls;
        ComponentInfo activeComponent;
        chrono::steady_clock::time_point lastStartTime;
        chrono::steady_clock::time_point lastEndTime;
        thread t;
    public:
        ComponentManager();
        ~ComponentManager();

        void init(const string& dllPath);
        
        ComponentInfo getActiveComponentInfo() const;
        
        template<typename Interface, typename ...Args>
        void exec(const ComponentInfo& componentInfo, Args... args) {
            auto component = getServer().componentFactory.createComponent<Interface>(componentInfo.type, componentInfo.name);
            activeComponent = componentInfo;
            this->state = State::READY;
            try {
                t = thread(&Interface::exec,
                    component, 
                    [this]() -> void {
                        this->lastStartTime = chrono::steady_clock::now();
                        this->state.store(State::RUNNING, std::memory_order_release);
                    },
                    [this]() -> void {
                        // Publish the timestamp before FINISH. The UI's acquire
                        // load then observes completed timing data, never stale data.
                        this->lastEndTime = chrono::steady_clock::now();
                        this->state.store(State::FINISH, std::memory_order_release);
                    },
                    std::forward<Args>(args)...);
                    t.detach();
            }
            catch(const exception& e) {
                cerr<<"Unexpected termination"<<endl;
                cerr<<e.what()<<endl;
            }
        }
    
        void finish();

        State getState() const;

        chrono::duration<double> getLastExecTime() const;
    };
} // namespace NRenderer


#endif
