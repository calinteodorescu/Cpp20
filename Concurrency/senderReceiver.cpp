// senderReceiver.cpp

#include <experimental/coroutine>
#include <chrono>
#include <iostream>
#include <functional>
#include <string>
#include <stdexcept>
#include <atomic>
#include <thread>

struct Task
{
    struct promise_type
    {
        Task get_return_object( )
        { 
            return { };
        }

        std::experimental::suspend_never initial_suspend( )
        { 
            return { };
        }

        std::experimental::suspend_never final_suspend ( ) noexcept 
        { 
            return { };
        }

        void return_void( )
        {
        }

        void unhandled_exception( )
        {
        }
    };
};

class Event 
{
 public:

    Event           ( )              = default;

    Event           ( const Event& ) = delete;
    Event           ( Event&&      ) = delete;
    Event& operator=( const Event& ) = delete;
    Event& operator=( Event&&      ) = delete;

    class Awaiter;
    Awaiter operator co_await( ) const noexcept;

    void notify( ) noexcept;

 private:

    friend class Awaiter;
  
    mutable std::atomic< void* > m_suspendedWaiter{ nullptr };
    mutable std::atomic< bool  > m_notified       { false   };
};

class Event::Awaiter 
{
 public:
    Awaiter( const Event& event )
    :   m_event( event )
    {
    }

    bool await_ready  ( ) const;
    bool await_suspend( std::experimental::coroutine_handle<> coroutineHandle ) noexcept;
    void await_resume ( ) noexcept;

 private:

    friend class Event;

    const Event&                          m_event;
    std::experimental::coroutine_handle<> m_coroutineHandle;
};

Event::Awaiter Event::operator co_await( ) const noexcept 
{
    return Awaiter{ * this };
}

bool Event::Awaiter::await_ready( ) const
{  
    // allow at most one waiter
    if ( m_event.m_suspendedWaiter.load( ) != nullptr )
    {
        throw std::runtime_error( "More than one waiter is not valid" );
    }
  
    // event.m_notified == false; suspends the coroutine
    // event.m_notified == true; the coroutine is executed like a normal function

    return m_event.m_notified;
}

bool Event::Awaiter::await_suspend( std::experimental::coroutine_handle<> coroutineHandle ) noexcept 
{
    m_coroutineHandle = coroutineHandle;
  
    if ( m_event.m_notified )
    {
        return false;
    }
  
    // store the waiter for later notification
    m_event.m_suspendedWaiter.store( this );

    return true;
}

void Event::Awaiter::await_resume ( ) noexcept
{
}

void Event::notify( ) noexcept 
{
    m_notified = true;
  
    // try to load the waiter
    auto* waiter = static_cast< Awaiter* >( m_suspendedWaiter.load( ) );
 
    // check if a waiter is available
    if ( waiter != nullptr ) 
    {
        // resume the coroutine => await_resume
        waiter->m_coroutineHandle.resume( );
    }
}

Task receiver( Event& event )
{ 
    auto start = std::chrono::high_resolution_clock::now( );

    co_await event; 

    std::cout << "Got the notification! " << '\n';
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end - start;
    std::cout << "Waited " << elapsed.count() << " seconds." << '\n';
}

using namespace std::chrono_literals;

int main( )
{    
    {
        std::cout << '\n';
        std::cout << "Notification before waiting" << '\n';

        Event event{ };
        auto senderThread   = std::thread( [ & event ]
                                           {
                                               event.notify( );
                                           }
                                         );
        auto receiverThread = std::thread( receiver,
                                           std::ref( event )
                                         );
    
        receiverThread.join( );
        senderThread.  join( );
    }
    
    {
        std::cout << '\n';
        std::cout << "Notification after 2 seconds waiting" << '\n';

        Event event{ };
        auto receiverThread = std::thread( receiver,
                                           std::ref( event )
                                         );
        auto senderThread   = std::thread( [ & event ]
                                           {
                                               std::this_thread::sleep_for( 2s );

                                               event.notify( );
                                           }
                                         );    
        receiverThread.join( );
        senderThread.  join( );
    }
     
    std::cout << '\n';    
}