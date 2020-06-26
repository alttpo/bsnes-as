
int lastConnect = 1;

void pre_frame() {
  if (!discord::created) {
    if (--lastConnect > 0) return;

    // attempt reconnect every 15 seconds:
    lastConnect = 60 * 10;

    //discord::logLevel = 4;  // DEBUG
    if (discord::create(725722436351426643) != 0) {
      message("discord::create() failed with err=" + fmtInt(discord::result));
      return;
    }

    message("discord: connected!");
    auto activity = discord::Activity();
    activity.Type = 1;
    discord::activityManager.UpdateActivity(activity, null);
  }
  if (!discord::created) return;

  if (discord::runCallbacks() != 0) {
    message("discord::runCallbacks() failed with err=" + fmtInt(discord::result));
    return;
  }
}
