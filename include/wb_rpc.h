/*
 * Copyright (c) 2020 SAP Labs France, d-shop Caen
 * All rights reserved
 *
 * Licensed under the Apache License, Version 2.0 (the ""License"");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an ""AS IS"" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*
 * RPC library.
 */

#ifndef wb_rpc_h
#define wb_rpc_h

void rpc_init();

void rpc_wallbox_get_info_handler(struct mg_rpc_request_info *, void *, struct mg_rpc_frame_info *, struct mg_str);

void rpc_wallbox_reboot_handler(struct mg_rpc_request_info *, void *, struct mg_rpc_frame_info *, struct mg_str);

void rpc_wallbox_reset_handler(struct mg_rpc_request_info *, void *, struct mg_rpc_frame_info *, struct mg_str);

#endif /* wb_rpc_h */
