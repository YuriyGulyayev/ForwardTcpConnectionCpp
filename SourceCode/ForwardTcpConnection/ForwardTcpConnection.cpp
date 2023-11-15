// Parts of this code come from the "trivial_portforward.cpp" at "http://www.quantumg.net/portforward.php".

// yg? Can we use the overlapped stuff? Completion ports?
// yg? Those could actually be slower for few connections.

// yg? The console output functions, when calledd on multiple threads at a time, can mix up their output.

#include "StdAfx.h"

struct TSocketPair
{
   int ReferenceCounter;
   SOCKET SocketsHandle[ 2U ];
};

long ConnectTimeoutTimeSpanMicroseconds;
int NumberThreads( 0 );
DWORD LastWorkTimeStampMilliseconds;

inline void AbortIfFalse
   ( bool bool1
   )
{
   if( bool1 )
   {
   }
   else
   {
      abort();
   }
}

DWORD WINAPI ForwardFromDestinationThreadProc
   ( _In_ LPVOID parameter1
   )
{










   TSocketPair & socketPair( * static_cast< TSocketPair * >( parameter1 ) );















   {




      //DWORD tickCount( /*GetTickCount()*/ timeGetTime() );

      for( ; ; )
      {
         // yg? Consider increasing buffer size. It should be configurable.
         char buffer1[ 1024U * 64U ];

         // yg? Using default receive timeout, which is infinity.
         // yg? After a timeout the socket is left in an indeterminate state and therefore must not be used any more but another thread can use it.
         int n1( recv( socketPair.SocketsHandle[ 1U ], buffer1, static_cast< int >( sizeof( buffer1 ) ), 0 ) );

         //_cwprintf( L"3: %i %i %i\n", static_cast< int >( /*GetTickCount()*/ timeGetTime() - tickCount ), n1, WSAGetLastError() );
         if( n1 <= 0 )
         {
            break;
         }




         // yg? The docs says this can send fewer bytes but we assume it won't happen. We should be able to send in one shot what we received in one shot.
         int n2( send( socketPair.SocketsHandle[ 0U ], buffer1, n1, 0 ) );

         //_cwprintf( L"\n4: %i\n", n2 );
         if( n2 != n1 )
         {
            break;
         }
      }

      // This interrupts {recv} on another thread.
      // This call can fail and/or might not be needed if the socket already has some problems, {send} above failed, or another thread already broke the loop.
      // There might be some marginal cases when this won't work. The keep-alive should help in those cases.
      shutdown( socketPair.SocketsHandle[ 0U ], SD_BOTH );
   }





   static_assert( sizeof( LONG ) == sizeof( socketPair.ReferenceCounter ), "" );
   if( InterlockedDecrementRelease( reinterpret_cast< LONG * >( & socketPair.ReferenceCounter ) ) <= 0 )
   {

      // Closing the accepted socket first to let the connecting app know sooner.
      // yg? But {shutdown} probably already notified the connecting app.
      // yg? {TSocketPair} needs a destructor to do this.
      AbortIfFalse( closesocket( socketPair.SocketsHandle[ 0U ] ) == 0 );
      AbortIfFalse( closesocket( socketPair.SocketsHandle[ 1U ] ) == 0 );

      delete ( & socketPair );
      //_cputws( L"deleted\n" );
   }

   // We must read this before crossing a release memory barrier.
   DWORD lastWorkTimeSpanMilliseconds1( LastWorkTimeStampMilliseconds );

   static_assert( sizeof( LONG ) == sizeof( NumberThreads ), "" );
   //_cwprintf( L"%i threads\n", static_cast< int >( InterlockedDecrementAcquire( reinterpret_cast< LONG * >( & NumberThreads ) ) ) );
   if( InterlockedDecrementRelease( reinterpret_cast< LONG * >( & NumberThreads ) ) <= 0 )
   {
      //_cputws( L"idle\n" );
      lastWorkTimeSpanMilliseconds1 = /*GetTickCount()*/ timeGetTime() - lastWorkTimeSpanMilliseconds1;
      _cwprintf( L"idle; %u\n", static_cast< unsigned int >( lastWorkTimeSpanMilliseconds1 ) );
   }

   return 0U;
}

DWORD WINAPI ForwardToDestinationThreadProc
   ( _In_ LPVOID parameter1
   )
{
   static_assert( sizeof( LONG ) == sizeof( NumberThreads ), "" );
   //_cwprintf( L"%i threads\n", static_cast< int >( InterlockedIncrementAcquire( reinterpret_cast< LONG * >( & NumberThreads ) ) ) );
   if( InterlockedIncrementAcquire( reinterpret_cast< LONG * >( & NumberThreads ) ) == 1 )
   {
      // yg? This is a low resolution timestamp.
      LastWorkTimeStampMilliseconds = /*GetTickCount()*/ timeGetTime();

      _cputws( L"working\n" );
   }

   TSocketPair & socketPair( * static_cast< TSocketPair * >( parameter1 ) );

   fd_set socketsReadyForWritingHandle;
   socketsReadyForWritingHandle.fd_count = 1U;
   socketsReadyForWritingHandle.fd_array[ 0U ] = socketPair.SocketsHandle[ 1U ];
   fd_set socketsWithErrorsHandle;
   socketsWithErrorsHandle.fd_count = 2U;
   socketsWithErrorsHandle.fd_array[ 0U ] = socketPair.SocketsHandle[ 0U ];
   socketsWithErrorsHandle.fd_array[ 1U ] = socketPair.SocketsHandle[ 1U ];
   timeval timeval1 = { 0, ConnectTimeoutTimeSpanMicroseconds };
   
   // Will {select} ever return if the connection was already established and then lost? This doesn't matter any more as we now use a timeout.
   // yg? Would it be better to use {WSAPoll}?
   AbortIfFalse( select( 0, nullptr, ( & socketsReadyForWritingHandle ), ( & socketsWithErrorsHandle ), ( & timeval1 ) ) != SOCKET_ERROR );

   if( socketsReadyForWritingHandle.fd_count == 1U && socketsWithErrorsHandle.fd_count == 0U )
   {
      static_assert( sizeof( LONG ) == sizeof( NumberThreads ), "" );
      InterlockedIncrementAcquire( reinterpret_cast< LONG * >( & NumberThreads ) );
      HANDLE threadHandle( CreateThread( nullptr, 0U, ( & ForwardFromDestinationThreadProc ), parameter1, 0U, nullptr ) );
      AbortIfFalse( threadHandle != nullptr );
      AbortIfFalse( CloseHandle( threadHandle ) != FALSE );

      for( ; ; )
      {
         // yg? Consider increasing buffer size. It should be configurable.
         char buffer1[ 1024U * 64U ];

         // yg? Using default receive timeout, which is infinity.
         // yg? After a timeout the socket is left in an indeterminate state and therefore must not be used any more but another thread can use it.
         int n1( recv( socketPair.SocketsHandle[ 0U ], buffer1, static_cast< int >( sizeof( buffer1 ) ), 0 ) );

         //_cwprintf( L"\n1: %i %i\n", n1, WSAGetLastError() );
         if( n1 <= 0 )
         {
            break;
         }

         // yg? For maximum performance, we could call {select} before we call {send} for the 1st time.
         // yg? Or beter call {send} first and if it fails call {select}.
         // yg? But we would need to ensure correct behavior in case the socket has already connected and then the connection broke.
         // yg? The docs says this can send fewer bytes but we assume it won't happen. We should be able to send in one shot what we received in one shot.
         int n2( send( socketPair.SocketsHandle[ 1U ], buffer1, n1, 0 ) );

         //_cwprintf( L"\n2: %i\n", n2 );
         if( n2 != n1 )
         {
            break;
         }
      }

      // This interrupts {recv} on another thread.
      // This call can fail and/or might not be needed if the socket already has some problems, {send} above failed, or another thread already broke the loop.
      // There might be some marginal cases when this won't work. The keep-alive should help in those cases.
      shutdown( socketPair.SocketsHandle[ 1U ], SD_BOTH );
   }
   else
   {
      goto label1;
   }

   static_assert( sizeof( LONG ) == sizeof( socketPair.ReferenceCounter ), "" );
   if( InterlockedDecrementRelease( reinterpret_cast< LONG * >( & socketPair.ReferenceCounter ) ) <= 0 )
   {
   label1:
      // Closing the accepted socket first to let the connecting app know sooner.
      //
      // yg? {TSocketPair} needs a destructor to do this.
      AbortIfFalse( closesocket( socketPair.SocketsHandle[ 0U ] ) == 0 );
      AbortIfFalse( closesocket( socketPair.SocketsHandle[ 1U ] ) == 0 );

      delete ( & socketPair );
      //_cputws( L"deleted\n" );
   }

   // We must read this before crossing a release memory barrier.
   DWORD lastWorkTimeSpanMilliseconds1( LastWorkTimeStampMilliseconds );

   static_assert( sizeof( LONG ) == sizeof( NumberThreads ), "" );
   //_cwprintf( L"%i threads\n", static_cast< int >( InterlockedDecrementAcquire( reinterpret_cast< LONG * >( & NumberThreads ) ) ) );
   if( InterlockedDecrementRelease( reinterpret_cast< LONG * >( & NumberThreads ) ) <= 0 )
   {
      //_cputws( L"idle\n" );
      lastWorkTimeSpanMilliseconds1 = /*GetTickCount()*/ timeGetTime() - lastWorkTimeSpanMilliseconds1;
      _cwprintf( L"idle; %u\n", static_cast< unsigned int >( lastWorkTimeSpanMilliseconds1 ) );
   }

   return 0U;
}

//int _tmain
//   ( int argc,
//     _TCHAR * argv[]
//   )
int main
   ( int argc,
// TODO 3 yg? Should this be something like {char const * const []}?
     char * argv[]
   )
{
#if( 1 )
#else
   #pragma message( "yg?? Forcing a trial period." )
   {
      SYSTEMTIME systemTime1;
      GetSystemTime( & systemTime1 );
      if( ( static_cast< unsigned int >( systemTime1.wYear ) << 4U ) + static_cast< unsigned int >( systemTime1.wMonth ) >= ( 2013U << 4U ) + 3U )
      {
         _cputws( L"trial period expired\n" );
         return EXIT_FAILURE;
      }
   }
#endif

   if( argc != 7 )
   {
      _cputws
         ( L"Usage: ForwardTcpConnection.exe port_to_listen_on IP_address_to_connect_to port_to_connect_to connect_timeout_milliseconds keep_alive_timeout_milliseconds keep_alive_interval_milliseconds\n"
           L"When the {keep_alive_timeout_milliseconds} is negative the keep-alive feature will not be used.\n"
         );
      return EXIT_FAILURE;
   }

   union
   {
      int int1;
      u_long u_long1;
   };

   // Among other things, this increases precision of {select} timeout.
   // No need to call {timeEndPeriod}.
   AbortIfFalse( timeBeginPeriod( 1U ) == static_cast< MMRESULT >( TIMERR_NOERROR ) );

   int portNumberToListenOn( atoi( argv[ 1U ] ) );
   int portNumberToConnectTo( atoi( argv[ 3U ] ) );

   // Assuming this will not overflow.
   ConnectTimeoutTimeSpanMicroseconds = atoi( argv[ 4U ] ) * 1000L;

   // yg? These are used for both incoming and outgoing connections.
   int keepAliveTimeoutTimeSpanMilliseconds( atoi( argv[ 5U ] ) );
   int keepAliveIntervalMilliseconds( atoi( argv[ 6U ] ) );

   WSADATA wsaData;
   AbortIfFalse( WSAStartup( MAKEWORD( 2, 2 ), ( & wsaData ) ) == 0 );

   SOCKET listeningSocketHandle( socket( AF_INET, SOCK_STREAM, IPPROTO_TCP ) );
   AbortIfFalse( listeningSocketHandle != INVALID_SOCKET );
   //AbortIfFalse( shutdown( listeningSocketHandle, SD_BOTH ) == 0 );

   sockaddr_in sin;
   sin.sin_family = AF_INET;
   sin.sin_port = htons( static_cast< u_short >( portNumberToListenOn ) );

   // yg? Any IP address. We don't support listening on a specific IP address.
   sin.sin_addr.S_un.S_addr = INADDR_ANY;

   // yg? Not explicitly enforcing exclusive address use.
   if( bind( listeningSocketHandle, reinterpret_cast< sockaddr const * >( & sin ), static_cast< int >( sizeof( sin ) ) ) != 0 )

   {
      _cputws( L"{bind} failed\n" );
      return EXIT_FAILURE;
   }
   AbortIfFalse( listen( listeningSocketHandle, SOMAXCONN ) == 0 );
   _cputws( L"idle\n" );

   // Infinite loop.
   for( ; ; )
   {
      //int1 = static_cast< int >( sizeof( sin ) );

      // This can fail but rarely does.
      SOCKET acceptedSocketHandle( accept( listeningSocketHandle, /*reinterpret_cast< sockaddr const * >( & sin )*/ nullptr, /*( & int1 )*/ nullptr ) );

      if( acceptedSocketHandle == INVALID_SOCKET )
      {
         _cputws( L"{accept} failed\n" );

         // Preventing 100% CPU use in case something breaks.
         Sleep( 1U );

         continue;
      }
      //printf("received connection from %i.%i.%i.%i\n", sin.sin_addr.S_un.S_un_b.s_b1, sin.sin_addr.S_un.S_un_b.s_b2, sin.sin_addr.S_un.S_un_b.s_b3, sin.sin_addr.S_un.S_un_b.s_b4);

      SOCKET connectedSocketHandle( socket( AF_INET, SOCK_STREAM, IPPROTO_TCP ) );
      AbortIfFalse( connectedSocketHandle != INVALID_SOCKET );
      int1 = 1;
      AbortIfFalse( setsockopt( connectedSocketHandle, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast< char const * >( & int1 ), static_cast< int >( sizeof( int1 ) ) ) == 0 );
      //AbortIfFalse( setsockopt( connectedSocketHandle, SOL_SOCKET, TCP_NODELAY, reinterpret_cast< char const * >( & int1 ), static_cast< int >( sizeof( int1 ) ) ) == 0 );

      if( keepAliveTimeoutTimeSpanMilliseconds >= 0 )
      {
         tcp_keepalive tcpKeepAlive1;
         tcpKeepAlive1.onoff = 1U;
         tcpKeepAlive1.keepalivetime = static_cast< unsigned int >( keepAliveTimeoutTimeSpanMilliseconds );
         tcpKeepAlive1.keepaliveinterval = static_cast< unsigned int >( keepAliveIntervalMilliseconds );
         DWORD bytesReturned;
         AbortIfFalse
            ( WSAIoctl
                  ( connectedSocketHandle,
                    SIO_KEEPALIVE_VALS,
                    ( & tcpKeepAlive1 ),
                    static_cast< DWORD >( sizeof( tcpKeepAlive1 ) ),
                    nullptr,
                    0U,
                    ( & bytesReturned ),
                    nullptr,
                    nullptr
                  ) ==
              0
            );
      }

      u_long1 = 1U;
      AbortIfFalse( ioctlsocket( connectedSocketHandle, FIONBIO, ( & u_long1 ) ) == 0 );

      sin.sin_family = AF_INET;
      sin.sin_port = htons( static_cast< u_short >( portNumberToConnectTo ) );

      // yg? Bug:
      // <Quote>
      // In some applications, you may want to explicitly check the input string for the broadcast address "255.255.255.255,"
      // since the return value from inet_addr() for this address is the same as SOCKET_ERROR.
      // </Quote>
      sin.sin_addr.S_un.S_addr = inet_addr( argv[ 2U ] );
      
      // A nonblocking connection initiation is unlikely to fail. It can only fail if arguments are invalid. So we could simply die if this fails.
      // Actually it can fail if the network or host is unreachable.
      if( connect( connectedSocketHandle, reinterpret_cast< sockaddr const * >( & sin ), static_cast< int >( sizeof( sin ) ) ) == 0 || WSAGetLastError() == WSAEWOULDBLOCK )

      {
      }
      else
      {
         // Closing the accepted socket first to let the connecting app know sooner.
         AbortIfFalse( closesocket( acceptedSocketHandle ) == 0 );
         AbortIfFalse( closesocket( connectedSocketHandle ) == 0 );

         _cputws( L"{connect} failed\n" );
         continue;
      }

      u_long1 = 0U;
      AbortIfFalse( ioctlsocket( connectedSocketHandle, FIONBIO, ( & u_long1 ) ) == 0 );

      int1 = 1;
      AbortIfFalse( setsockopt( acceptedSocketHandle, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast< char const * >( & int1 ), static_cast< int >( sizeof( int1 ) ) ) == 0 );
      //AbortIfFalse( setsockopt( acceptedSocketHandle, SOL_SOCKET, TCP_NODELAY, reinterpret_cast< char const * >( & int1 ), static_cast< int >( sizeof( int1 ) ) ) == 0 );

      if( keepAliveTimeoutTimeSpanMilliseconds >= 0 )
      {
         tcp_keepalive tcpKeepAlive1;
         tcpKeepAlive1.onoff = 1U;
         tcpKeepAlive1.keepalivetime = static_cast< unsigned int >( keepAliveTimeoutTimeSpanMilliseconds );
         tcpKeepAlive1.keepaliveinterval = static_cast< unsigned int >( keepAliveIntervalMilliseconds );
         DWORD bytesReturned;
         AbortIfFalse
            ( WSAIoctl
                  ( acceptedSocketHandle,
                    SIO_KEEPALIVE_VALS,
                    ( & tcpKeepAlive1 ),
                    static_cast< DWORD >( sizeof( tcpKeepAlive1 ) ),
                    nullptr,
                    0U,
                    ( & bytesReturned ),
                    nullptr,
                    nullptr
                  ) ==
              0
            );
      }

      // yg? Assuming if this blows up the app will die even if some threads are still running. {abort} will be called, right?
      TSocketPair * socketPair( new TSocketPair );

      socketPair->ReferenceCounter = 2;
      socketPair->SocketsHandle[ 0U ] = acceptedSocketHandle;
      socketPair->SocketsHandle[ 1U ] = connectedSocketHandle;
      HANDLE threadHandle;
      threadHandle = CreateThread( nullptr, 0U, ( & ForwardToDestinationThreadProc ), socketPair, 0U, nullptr );
      AbortIfFalse( threadHandle != nullptr );
      AbortIfFalse( CloseHandle( threadHandle ) != FALSE );
      //threadHandle = CreateThread( nullptr, 0U, ( & ForwardFromDestinationThreadProc ), socketPair, 0U, nullptr );
      //AbortIfFalse( threadHandle != nullptr );
      //AbortIfFalse( CloseHandle( threadHandle ) != FALSE );
   }

   //AbortIfFalse( closesocket( listeningSocketHandle ) == 0 );
   //return EXIT_SUCCESS;
}
