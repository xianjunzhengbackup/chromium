// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "app/test/data/resource.h"
#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/scoped_ptr.h"
#include "base/values.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/pref_service.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/json_value_serializer.h"
#include "chrome/common/notification_observer_mock.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/notification_type.h"
#include "chrome/common/pref_names.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Mock;
using testing::Pointee;
using testing::Property;

class PrefServiceTest : public testing::Test {
 protected:
  virtual void SetUp() {
    // Name a subdirectory of the temp directory.
    ASSERT_TRUE(PathService::Get(base::DIR_TEMP, &test_dir_));
    test_dir_ = test_dir_.AppendASCII("PrefServiceTest");

    // Create a fresh, empty copy of this directory.
    file_util::Delete(test_dir_, true);
    file_util::CreateDirectory(test_dir_);

    ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &data_dir_));
    data_dir_ = data_dir_.AppendASCII("pref_service");
    ASSERT_TRUE(file_util::PathExists(data_dir_));
  }
  virtual void TearDown() {
    // Clean up test directory
    ASSERT_TRUE(file_util::Delete(test_dir_, true));
    ASSERT_FALSE(file_util::PathExists(test_dir_));
  }

  // the path to temporary directory used to contain the test operations
  FilePath test_dir_;
  // the path to the directory where the test data is stored
  FilePath data_dir_;
};

class TestPrefObserver : public NotificationObserver {
 public:
  TestPrefObserver(const PrefService* prefs, const std::wstring& pref_name,
      const std::wstring& new_pref_value)
      : observer_fired_(false),
        prefs_(prefs),
        pref_name_(pref_name),
        new_pref_value_(new_pref_value) {
  }
  virtual ~TestPrefObserver() {}

  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details) {
    EXPECT_EQ(type.value, NotificationType::PREF_CHANGED);
    PrefService* prefs_in = Source<PrefService>(source).ptr();
    EXPECT_EQ(prefs_in, prefs_);
    std::wstring* pref_name_in = Details<std::wstring>(details).ptr();
    EXPECT_EQ(*pref_name_in, pref_name_);
    EXPECT_EQ(new_pref_value_, prefs_in->GetString(L"homepage"));
    observer_fired_ = true;
  }

  bool observer_fired() { return observer_fired_; }

  void Reset(const std::wstring& new_pref_value) {
    observer_fired_ = false;
    new_pref_value_ = new_pref_value;
  }

 private:
  bool observer_fired_;
  const PrefService* prefs_;
  const std::wstring pref_name_;
  std::wstring new_pref_value_;
};

TEST_F(PrefServiceTest, Basic) {
  {
    // Test that it fails on nonexistent file.
    FilePath bogus_input_file = data_dir_.AppendASCII("read.txt");
    PrefService prefs(bogus_input_file);
    EXPECT_FALSE(prefs.ReloadPersistentPrefs());
  }

  ASSERT_TRUE(file_util::CopyFile(data_dir_.AppendASCII("read.json"),
                                  test_dir_.AppendASCII("write.json")));

  // Test that the persistent value can be loaded.
  FilePath input_file = test_dir_.AppendASCII("write.json");
  ASSERT_TRUE(file_util::PathExists(input_file));
  PrefService prefs(input_file);
  ASSERT_TRUE(prefs.ReloadPersistentPrefs());

  // Register test prefs.
  const wchar_t kNewWindowsInTabs[] = L"tabs.new_windows_in_tabs";
  const wchar_t kMaxTabs[] = L"tabs.max_tabs";
  const wchar_t kLongIntPref[] = L"long_int.pref";
  prefs.RegisterStringPref(prefs::kHomePage, L"");
  prefs.RegisterBooleanPref(kNewWindowsInTabs, false);
  prefs.RegisterIntegerPref(kMaxTabs, 0);
  prefs.RegisterStringPref(kLongIntPref, L"2147483648");

  std::wstring microsoft(L"http://www.microsoft.com");
  std::wstring cnn(L"http://www.cnn.com");
  std::wstring homepage(L"http://www.example.com");

  EXPECT_EQ(cnn, prefs.GetString(prefs::kHomePage));

  const wchar_t kSomeDirectory[] = L"some_directory";
  FilePath some_path(FILE_PATH_LITERAL("/usr/sbin/"));
  prefs.RegisterFilePathPref(kSomeDirectory, FilePath());

  // Test reading some other data types from sub-dictionaries, and
  // writing to the persistent store.
  EXPECT_TRUE(prefs.GetBoolean(kNewWindowsInTabs));
  prefs.SetBoolean(kNewWindowsInTabs, false);
  EXPECT_FALSE(prefs.GetBoolean(kNewWindowsInTabs));

  EXPECT_EQ(20, prefs.GetInteger(kMaxTabs));
  prefs.SetInteger(kMaxTabs, 10);
  EXPECT_EQ(10, prefs.GetInteger(kMaxTabs));

  EXPECT_EQ(2147483648LL, prefs.GetInt64(kLongIntPref));
  prefs.SetInt64(kLongIntPref, 214748364842LL);
  EXPECT_EQ(214748364842LL, prefs.GetInt64(kLongIntPref));

  EXPECT_EQ(FilePath::StringType(FILE_PATH_LITERAL("/usr/local/")),
      prefs.GetFilePath(kSomeDirectory).value());
  prefs.SetFilePath(kSomeDirectory, some_path);
  EXPECT_EQ(some_path.value(), prefs.GetFilePath(kSomeDirectory).value());

  // Serialize and compare to expected output.
  // SavePersistentPrefs uses ImportantFileWriter which needs a file thread.
  MessageLoop message_loop;
  ChromeThread file_thread(ChromeThread::FILE, &message_loop);
  FilePath output_file = test_dir_.AppendASCII("write.json");
  FilePath golden_output_file = data_dir_.AppendASCII("write.golden.json");
  ASSERT_TRUE(file_util::PathExists(golden_output_file));
  ASSERT_TRUE(prefs.SavePersistentPrefs());
  MessageLoop::current()->RunAllPending();
  EXPECT_TRUE(file_util::TextContentsEqual(golden_output_file, output_file));
  ASSERT_TRUE(file_util::Delete(output_file, false));
}

TEST_F(PrefServiceTest, Observers) {
  FilePath input_file = data_dir_.AppendASCII("read.json");
  EXPECT_TRUE(file_util::PathExists(input_file));

  PrefService prefs(input_file);

  EXPECT_TRUE(prefs.ReloadPersistentPrefs());

  const wchar_t pref_name[] = L"homepage";
  prefs.RegisterStringPref(pref_name, L"");
  EXPECT_EQ(std::wstring(L"http://www.cnn.com"), prefs.GetString(pref_name));

  const std::wstring new_pref_value(L"http://www.google.com/");
  TestPrefObserver obs(&prefs, pref_name, new_pref_value);
  prefs.AddPrefObserver(pref_name, &obs);
  // This should fire the checks in TestPrefObserver::Observe.
  prefs.SetString(pref_name, new_pref_value);

  // Make sure the tests were actually run.
  EXPECT_TRUE(obs.observer_fired());

  // Now try adding a second pref observer.
  const std::wstring new_pref_value2(L"http://www.youtube.com/");
  obs.Reset(new_pref_value2);
  TestPrefObserver obs2(&prefs, pref_name, new_pref_value2);
  prefs.AddPrefObserver(pref_name, &obs2);
  // This should fire the checks in obs and obs2.
  prefs.SetString(pref_name, new_pref_value2);
  EXPECT_TRUE(obs.observer_fired());
  EXPECT_TRUE(obs2.observer_fired());

  // Make sure obs2 still works after removing obs.
  prefs.RemovePrefObserver(pref_name, &obs);
  obs.Reset(L"");
  obs2.Reset(new_pref_value);
  // This should only fire the observer in obs2.
  prefs.SetString(pref_name, new_pref_value);
  EXPECT_FALSE(obs.observer_fired());
  EXPECT_TRUE(obs2.observer_fired());

  // Ok, clean up.
  prefs.RemovePrefObserver(pref_name, &obs2);
}

// TODO(port): port this test to POSIX.
#if defined(OS_WIN)
TEST_F(PrefServiceTest, LocalizedPrefs) {
  PrefService prefs((FilePath()));
  const wchar_t kBoolean[] = L"boolean";
  const wchar_t kInteger[] = L"integer";
  const wchar_t kString[] = L"string";
  prefs.RegisterLocalizedBooleanPref(kBoolean, IDS_LOCALE_BOOL);
  prefs.RegisterLocalizedIntegerPref(kInteger, IDS_LOCALE_INT);
  prefs.RegisterLocalizedStringPref(kString, IDS_LOCALE_STRING);

  // The locale default should take preference over the user default.
  EXPECT_FALSE(prefs.GetBoolean(kBoolean));
  EXPECT_EQ(1, prefs.GetInteger(kInteger));
  EXPECT_EQ(L"hello", prefs.GetString(kString));

  prefs.SetBoolean(kBoolean, true);
  EXPECT_TRUE(prefs.GetBoolean(kBoolean));
  prefs.SetInteger(kInteger, 5);
  EXPECT_EQ(5, prefs.GetInteger(kInteger));
  prefs.SetString(kString, L"foo");
  EXPECT_EQ(L"foo", prefs.GetString(kString));
}
#endif

TEST_F(PrefServiceTest, NoObserverFire) {
  PrefService prefs((FilePath()));

  const wchar_t pref_name[] = L"homepage";
  prefs.RegisterStringPref(pref_name, L"");

  const std::wstring new_pref_value(L"http://www.google.com/");
  TestPrefObserver obs(&prefs, pref_name, new_pref_value);
  prefs.AddPrefObserver(pref_name, &obs);
  // This should fire the checks in TestPrefObserver::Observe.
  prefs.SetString(pref_name, new_pref_value);

  // Make sure the observer was actually fired.
  EXPECT_TRUE(obs.observer_fired());

  // Setting the pref to the same value should not set the pref value a second
  // time.
  obs.Reset(new_pref_value);
  prefs.SetString(pref_name, new_pref_value);
  EXPECT_FALSE(obs.observer_fired());

  // Clearing the pref should cause the pref to fire.
  obs.Reset(L"");
  prefs.ClearPref(pref_name);
  EXPECT_TRUE(obs.observer_fired());

  // Clearing the pref again should not cause the pref to fire.
  obs.Reset(L"");
  prefs.ClearPref(pref_name);
  EXPECT_FALSE(obs.observer_fired());

  // Ok, clean up.
  prefs.RemovePrefObserver(pref_name, &obs);
}

TEST_F(PrefServiceTest, HasPrefPath) {
  PrefService prefs((FilePath()));

  const wchar_t path[] = L"fake.path";

  // Shouldn't initially have a path.
  EXPECT_FALSE(prefs.HasPrefPath(path));

  // Register the path. This doesn't set a value, so the path still shouldn't
  // exist.
  prefs.RegisterStringPref(path, std::wstring());
  EXPECT_FALSE(prefs.HasPrefPath(path));

  // Set a value and make sure we have a path.
  prefs.SetString(path, L"blah");
  EXPECT_TRUE(prefs.HasPrefPath(path));
}

class PrefServiceSetValueTest : public testing::Test {
 protected:
  static const wchar_t name_[];
  static const wchar_t value_[];

  PrefServiceSetValueTest()
      : prefs_(FilePath()),
        name_string_(name_),
        null_value_(Value::CreateNullValue()) {}

  void SetExpectNoNotification() {
    EXPECT_CALL(observer_, Observe(_, _, _)).Times(0);
  }

  void SetExpectPrefChanged() {
    EXPECT_CALL(observer_,
                Observe(NotificationType(NotificationType::PREF_CHANGED), _,
                        Property(&Details<std::wstring>::ptr,
                                 Pointee(name_string_))));
  }

  PrefService prefs_;
  std::wstring name_string_;
  scoped_ptr<Value> null_value_;
  NotificationObserverMock observer_;
};
const wchar_t PrefServiceSetValueTest::name_[] = L"name";
const wchar_t PrefServiceSetValueTest::value_[] = L"value";

TEST_F(PrefServiceSetValueTest, SetStringValue) {
  const wchar_t default_string[] = L"default";
  scoped_ptr<Value> default_value(Value::CreateStringValue(default_string));
  prefs_.RegisterStringPref(name_, default_string);
  prefs_.AddPrefObserver(name_, &observer_);
  SetExpectNoNotification();
  prefs_.Set(name_, *default_value);
  Mock::VerifyAndClearExpectations(&observer_);

  scoped_ptr<Value> new_value(Value::CreateStringValue(value_));
  SetExpectPrefChanged();
  prefs_.Set(name_, *new_value);
  EXPECT_EQ(value_, prefs_.GetString(name_));

  prefs_.RemovePrefObserver(name_, &observer_);
}

TEST_F(PrefServiceSetValueTest, SetDictionaryValue) {
  prefs_.RegisterDictionaryPref(name_);
  prefs_.AddPrefObserver(name_, &observer_);

  SetExpectNoNotification();
  prefs_.Set(name_, *null_value_);
  Mock::VerifyAndClearExpectations(&observer_);

  DictionaryValue new_value;
  new_value.SetString(name_, value_);
  SetExpectPrefChanged();
  prefs_.Set(name_, new_value);
  Mock::VerifyAndClearExpectations(&observer_);
  DictionaryValue* dict = prefs_.GetMutableDictionary(name_);
  EXPECT_EQ(1U, dict->size());
  std::wstring out_value;
  dict->GetString(name_, &out_value);
  EXPECT_EQ(value_, out_value);

  SetExpectNoNotification();
  prefs_.Set(name_, new_value);
  Mock::VerifyAndClearExpectations(&observer_);

  SetExpectPrefChanged();
  prefs_.Set(name_, *null_value_);
  Mock::VerifyAndClearExpectations(&observer_);
  dict = prefs_.GetMutableDictionary(name_);
  EXPECT_EQ(0U, dict->size());

  prefs_.RemovePrefObserver(name_, &observer_);
}

TEST_F(PrefServiceSetValueTest, SetListValue) {
  prefs_.RegisterListPref(name_);
  prefs_.AddPrefObserver(name_, &observer_);

  SetExpectNoNotification();
  prefs_.Set(name_, *null_value_);
  Mock::VerifyAndClearExpectations(&observer_);

  ListValue new_value;
  new_value.Append(Value::CreateStringValue(value_));
  SetExpectPrefChanged();
  prefs_.Set(name_, new_value);
  Mock::VerifyAndClearExpectations(&observer_);
  ListValue* list = prefs_.GetMutableList(name_);
  ASSERT_EQ(1U, list->GetSize());
  std::wstring out_value;
  list->GetString(0, &out_value);
  EXPECT_EQ(value_, out_value);

  SetExpectNoNotification();
  prefs_.Set(name_, new_value);
  Mock::VerifyAndClearExpectations(&observer_);

  SetExpectPrefChanged();
  prefs_.Set(name_, *null_value_);
  Mock::VerifyAndClearExpectations(&observer_);
  list = prefs_.GetMutableList(name_);
  EXPECT_EQ(0U, list->GetSize());

  prefs_.RemovePrefObserver(name_, &observer_);
}
