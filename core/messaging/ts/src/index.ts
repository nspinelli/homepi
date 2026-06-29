export type {
  BrokerEvent,
  LegacyRpcRequest,
  LegacyRpcResponse,
  MessagingError,
  SocketRequest,
  SocketResponse,
} from "./messaging-types.js";
export {
  createErrorResponse,
  createMessagingError,
  createRequest,
  createSuccessResponse,
  parseLegacyResponseLine,
  parseResponseLine,
} from "./envelope.js";
export {
  createBrokerEvent,
  isUiVisibleEvent,
  type CreateBrokerEventInput,
} from "./event.js";
export {
  createCorrelationId,
  createEventId,
  createRequestId,
} from "./correlation.js";
export {
  DEFAULT_SOCKET_TIMEOUT_MS,
  isSocketReachable,
  legacyRpc,
  MAX_MESSAGE_BYTES,
  sendCommand,
  socketRpc,
  type LegacyRpcOptions,
  type SocketRpcOptions,
} from "./socket-client.js";
export {
  MessagingSocketServer,
  writeResponse,
  type CommandHandler,
  type SocketServerOptions,
} from "./socket-server.js";
