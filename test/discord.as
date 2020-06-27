
int lastConnect = 1;
int presenceCount = 1;
uint64 start = 0;

void init() {
  start = chrono::timestamp;
}

void pre_frame() {
  if (!discord::created) {
    if (--lastConnect > 0) return;

    // attempt reconnect every 10 seconds:
    lastConnect = 10 * 60;

    //discord::logLevel = 4;  // DEBUG
    if (discord::create(725722436351426643) != 0) {
      message("discord::create() failed with err=" + fmtInt(discord::result));
      return;
    }

    message("discord: connected!");
    start = chrono::timestamp;
  }
  if (!discord::created) return;

  if (--presenceCount <= 0) {
    // update presence every 20 seconds:
    presenceCount = 5 * 60;

    auto activity = discord::Activity();
    activity.Type = 1;
    activity.Details = "details";
    activity.State = "state";
    activity.Assets.LargeImage = "logo";
    activity.Assets.LargeText = "ALttPO";
    //activity.Assets.SmallImage = "logo";
    //activity.Assets.SmallText = "small text";
    activity.Timestamps.Start = start;
    activity.Instance = true;
    discord::activityManager.UpdateActivity(activity, null);
  }

  if (discord::runCallbacks() != 0) {
    message("discord::runCallbacks() failed with err=" + fmtInt(discord::result));
    return;
  }
}
