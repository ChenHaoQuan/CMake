#pragma once
#include <map>
#include <string>
#include <vector>
#include "BasicBreakpointManager.h"

class cmCommand;
class BasicIncomingSocket;
struct cmListFileFunction;

namespace Sysprogs
{
	class ReplyBuilder;
	class RequestReader;
	enum class HLDPPacketType;
	enum class TargetStopReason;

	class HLDPServer
	{
	public:
		HLDPServer(int tcpPort);
		~HLDPServer();

		typedef int UniqueScopeID, UniqueExpressionID;

		class RAIIScope
		{
		private:
			HLDPServer *m_pServer;
			UniqueScopeID m_UniqueID;

		public:
			cmCommand *Command;
			cmMakefile *Makefile;
			const cmListFileFunction &Function;
			std::string SourceFile;
			cmStateDetail::PositionType Position;
			UniqueScopeID GetUniqueID() { return m_UniqueID; }

		public:
			RAIIScope(HLDPServer *pServer, cmCommand *pCommand, cmMakefile *pMakefile, const cmListFileFunction &function);

			RAIIScope(const RAIIScope &) = delete;
			void operator=(const RAIIScope &) = delete;

			~RAIIScope();
		};

	public: // Public interface for debugged code
		bool WaitForClient();
		std::unique_ptr<RAIIScope> OnExecutingInitialPass(cmCommand *pCommand, cmMakefile *pMakefile, const cmListFileFunction &function);
		void OnMessageProduced(unsigned /*cmake::MessageType*/ type, const std::string &message);

	private:
		void HandleBreakpointRelatedCommand(HLDPPacketType type, RequestReader &reader);
		bool SendReply(HLDPPacketType packetType, const ReplyBuilder &builder);
		HLDPPacketType ReceiveRequest(RequestReader &reader); // Returns 'Invalid' on error
		void SendErrorPacket(std::string details);

		void ReportStopAndServeDebugRequests(TargetStopReason stopReason, unsigned intParam, const std::string &stringParam);

	private:
		class ExpressionBase
		{
		public:
			UniqueExpressionID AssignedID = -1;
			std::string Name, Value, Type;
			int ChildCountOrMinusOneIfNotYetComputed = 0;

		public:
			std::vector<UniqueExpressionID> RegisteredChildren;
			bool ChildrenRegistered = false;
			virtual std::vector<std::unique_ptr<ExpressionBase>> CreateChildren() { return std::vector<std::unique_ptr<ExpressionBase>>(); }

		public:
			virtual ~ExpressionBase() {}
		};

		class SimpleExpression;
		class TargetExpression;
		class TargetPropertyListExpression;

	private:
		std::unique_ptr<ExpressionBase> CreateExpression(const std::string &text, const RAIIScope &scope);

	private:
		BasicIncomingSocket *m_pSocket;
		bool m_BreakInPending = false;
		bool m_EventsReported = false;
		bool m_Detached = false;
		std::vector<RAIIScope *> m_CallStack;
		std::map<UniqueExpressionID, std::unique_ptr<ExpressionBase>> m_ExpressionCache;
		BasicBreakpointManager m_BreakpointManager;

		enum
		{
			kNoScope = -1,
			kRootScope = -2,
		};

		UniqueScopeID m_NextScopeID = 0;
		UniqueScopeID m_EndOfStepScopeID = kNoScope;

		UniqueExpressionID m_NextExpressionID = 0;
	};
} // namespace Sysprogs