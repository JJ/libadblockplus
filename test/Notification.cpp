/*
 * This file is part of Adblock Plus <https://adblockplus.org/>,
 * Copyright (C) 2006-present eyeo GmbH
 *
 * Adblock Plus is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * Adblock Plus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Adblock Plus.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "BaseJsTest.h"

using namespace AdblockPlus;

namespace
{
  class NotificationTest : public BaseJsTest
  {
  protected:
    void SetUp()
    {
      LazyFileSystem* fileSystem;
      ThrowingPlatformCreationParameters platformParams;
      platformParams.timer.reset(new NoopTimer());
      platformParams.fileSystem.reset(fileSystem = new LazyFileSystem());
      platformParams.webRequest.reset(new NoopWebRequest());
      platform.reset(new Platform(std::move(platformParams)));
      CreateFilterEngine(*fileSystem, *platform);
    }

    void AddNotification(const std::string& notification)
    {
      GetJsEngine().Evaluate("(function()"
      "{"
        "require('notifications').notifications.addNotification(" + notification + ");"
      "})();");
    }

    std::unique_ptr<Notification> PeekNotification()
    {
      std::unique_ptr<Notification> retValue;
      auto& filterEngine = platform->GetFilterEngine();
      filterEngine.SetShowNotificationCallback(
        [&retValue](Notification&& notification) {
          retValue.reset(new Notification(std::move(notification)));
        });
      filterEngine.ShowNextNotification();
      filterEngine.RemoveShowNotificationCallback();
      return retValue;
    }
  };

  class MockWebRequest : public IWebRequest
  {
  public:
    std::string responseText;
    explicit MockWebRequest(const std::string& notification)
      : responseText(notification)
    {
    }
    void GET(const std::string& url, const HeaderList& requestHeaders,
      const GetCallback& getCallback) override
    {
      if (url.find("/notification.json") == std::string::npos)
      {
        return;
      }
      ServerResponse serverResponse;
      serverResponse.status = IWebRequest::NS_OK;
      serverResponse.responseStatus = 200;
      serverResponse.responseText = responseText;
      getCallback(serverResponse);
    }
  };

  // To run this test one needs to set INITIAL_DELAY to about 2000 msec
  // in notifications.js.
  class NotificationMockWebRequestTest : public BaseJsTest
  {
  protected:
    bool isNotificationCallbackCalled;
    DelayedTimer::SharedTasks timerTasks;
    void SetUp()
    {
      isNotificationCallbackCalled = false;
      const char* responseJsonText = "{"
        "\"notifications\": [{"
          "\"id\": \"some id\","
          "\"type\": \"information\","
          "\"message\": {"
             "\"en-US\": \"message\""
          "},"
          "\"title\": \"Title\""
        "}]"
        "}";

      LazyFileSystem* fileSystem;
      ThrowingPlatformCreationParameters platformParams;
      platformParams.timer = DelayedTimer::New(timerTasks);
      platformParams.fileSystem.reset(fileSystem = new LazyFileSystem());
      platformParams.webRequest.reset(new MockWebRequest(responseJsonText));
      platform.reset(new Platform(std::move(platformParams)));

      CreateFilterEngine(*fileSystem, *platform);
      auto& filterEngine = platform->GetFilterEngine();
      filterEngine.SetShowNotificationCallback(
        [this](Notification&& notification) {
          isNotificationCallbackCalled = true;
          EXPECT_EQ(NotificationType::NOTIFICATION_TYPE_INFORMATION, notification.GetType());
          EXPECT_EQ("Title", notification.GetTexts().title);
          EXPECT_EQ("message", notification.GetTexts().message);
          notification.MarkAsShown();
        });
    }
  };
}

TEST_F(NotificationTest, NoNotifications)
{
  EXPECT_FALSE(PeekNotification());
}

TEST_F(NotificationMockWebRequestTest, SingleNotification)
{
  auto& filterEngine = platform->GetFilterEngine();
  auto ii = timerTasks->begin();
  while(!isNotificationCallbackCalled && ii != timerTasks->end()) {
    ii->callback();
    ii = timerTasks->erase(ii);
    filterEngine.ShowNextNotification();
  }
  EXPECT_TRUE(isNotificationCallbackCalled);
}

TEST_F(NotificationTest, AddNotification)
{
  AddNotification("{"
      "type: 'critical',"
      "title: 'testTitle',"
      "message: 'testMessage',"
    "}");
  auto notification = PeekNotification();
  ASSERT_TRUE(notification);
  EXPECT_EQ(NotificationType::NOTIFICATION_TYPE_CRITICAL, notification->GetType());
  EXPECT_EQ("testTitle", notification->GetTexts().title);
  EXPECT_EQ("testMessage", notification->GetTexts().message);
}

TEST_F(NotificationTest, MarkAsShown)
{
  AddNotification("{ id: 'id', type: 'information' }");
  EXPECT_TRUE(PeekNotification());
  auto notification = PeekNotification();
  ASSERT_TRUE(notification);
  notification->MarkAsShown();
  EXPECT_FALSE(PeekNotification());
}

TEST_F(NotificationTest, NoLinks)
{
  AddNotification("{ id: 'id'}");
  auto notification = PeekNotification();
  ASSERT_TRUE(notification);
  EXPECT_EQ(0u, notification->GetLinks().size());
}

TEST_F(NotificationTest, Links)
{
  AddNotification("{ id: 'id', links: ['link1', 'link2'] }");
  auto notification = PeekNotification();
  ASSERT_TRUE(notification);
  std::vector<std::string> notificationLinks = notification->GetLinks();
  ASSERT_EQ(2u, notificationLinks.size());
  EXPECT_EQ("link1", notificationLinks[0]);
  EXPECT_EQ("link2", notificationLinks[1]);
}
