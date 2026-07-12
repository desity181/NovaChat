# NovaChat V1 集成验证清单

**验证日期：** 2026-07-04  
**构建环境：** Windows 10, VS2022, Qt 6.8.3, CMake 3.29, vcpkg

---

## 单元测试结果

```
100% tests passed, 0 tests failed out of 43
Total Test time (real) = ~1.0s
```

| 测试套件 | 数量 | 状态 |
|---------|------|------|
| Logger | 2 | ✅ |
| AppConfig | 4 | ✅ |
| DatabaseManager | 5 | ✅ |
| ProtocolCodec | 6 | ✅ |
| UserRepository | 7 | ✅ |
| ContactRepository | 7 | ✅ |
| MessageRepository | 7 | ✅ |
| ConversationRepository | 5 | ✅ |

---

## 集成场景（需人工验证）

### 场景一：完整注册→登录→加好友→聊天
- [ ] 注册 alice，登录后主窗口显示「● Connected」
- [ ] 注册 bob，alice 搜索并添加 bob 为好友
- [ ] alice 双击 bob 联系人，发送「你好 Bob！」
- [ ] bob 实时收到消息，回复成功

### 场景二：重启恢复
- [ ] 关闭客户端后重启，自动登录（跳过 LoginWindow）
- [ ] 联系人列表和历史消息从本地 SQLite 恢复

### 场景三：断线重连
- [ ] 关闭服务端，客户端状态变为「● Disconnected」→「○ Reconnecting」
- [ ] 重启服务端，客户端自动重连并恢复「● Connected」

### 场景四：退出登录
- [ ] 「File → Settings」弹出账号信息对话框
- [ ] 点「Log Out」跳回 LoginWindow，重启后不自动登录

### 场景五：边界情况
- [ ] 空消息不发送
- [ ] 错误密码登录显示提示
- [ ] 重复添加好友显示「Already friends or user not found」

---

## V1 已知限制

- 服务端为纯内存存储，重启后数据丢失（预期行为）
- 不支持群聊、图片/文件传输、音视频
- 好友关系立即双向生效，无需确认
- Token 无过期时间（由服务端 session 管理）
- 不支持多服务器账号同时登录

---

## 修复记录

无（首次完整集成通过）
