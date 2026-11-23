// ClientFunctions.h

#pragma once

enum ClientFunction
{
	CL_POST_TEXTMESSAGE,   // std::string
	CL_POST_ERRORMESSAGE,  // std::string
	CL_POST_GAMEINFO,      // GameInfo
	CL_POST_SETID,         // unsigned short
	CL_POST_PLAYERQUIT,    // unsigned short
	CL_POST_CONTROL,       // ControlPacketVector
	CL_POST_PLAYER_READY,  // PlayerReady
	CL_POST_STARTGAME,     // bool -- dummy
	CL_POST_ADDBOT,        // BotDesc
	CL_POST_PLAYERINFO,    // PlayerDescEx
	CL_POST_LEADING,       // float -- how often (0..1) your packets were already waiting when a frame was sent
	CL_POST_YOUARELAST,    // bool -- your control packet was the last one the server waited for
};


// end of file
