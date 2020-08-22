auto PathSettings::create() -> void {
  setCollapsible();
  setVisible(false);

  layout.setSize({4, 7});
  layout.column(0).setAlignment(1.0);

  gamesLabel.setText("Games:");
  gamesPath.setEditable(false);
  gamesAssign.setText("Assign ...").onActivate([&] {
    if(auto location = program.selectPath()) {
      settings.path.games = location;
      refreshPaths();
    }
  });
  gamesReset.setText("Reset").onActivate([&] {
    settings.path.games = "";
    refreshPaths();
  });

  patchesLabel.setText("Patches:");
  patchesPath.setEditable(false);
  patchesAssign.setText("Assign ...").onActivate([&] {
    if(auto location = program.selectPath()) {
      settings.path.patches = location;
      refreshPaths();
    }
  });
  patchesReset.setText("Reset").onActivate([&] {
    settings.path.patches = "";
    refreshPaths();
  });

  savesLabel.setText("Saves:");
  savesPath.setEditable(false);
  savesAssign.setText("Assign ...").onActivate([&] {
    if(auto location = program.selectPath()) {
      settings.path.saves = location;
      refreshPaths();
    }
  });
  savesReset.setText("Reset").onActivate([&] {
    settings.path.saves = "";
    refreshPaths();
  });

  cheatsLabel.setText("Cheats:");
  cheatsPath.setEditable(false);
  cheatsAssign.setText("Assign ...").onActivate([&] {
    if(auto location = program.selectPath()) {
      settings.path.cheats = location;
      refreshPaths();
    }
  });
  cheatsReset.setText("Reset").onActivate([&] {
    settings.path.cheats = "";
    refreshPaths();
  });

  statesLabel.setText("States:");
  statesPath.setEditable(false);
  statesAssign.setText("Assign ...").onActivate([&] {
    if(auto location = program.selectPath()) {
      settings.path.states = location;
      refreshPaths();
    }
  });
  statesReset.setText("Reset").onActivate([&] {
    settings.path.states = "";
    refreshPaths();
  });

  screenshotsLabel.setText("Screenshots:");
  screenshotsPath.setEditable(false);
  screenshotsAssign.setText("Assign ...").onActivate([&] {
    if(auto location = program.selectPath()) {
      settings.path.screenshots = location;
      refreshPaths();
    }
  });
  screenshotsReset.setText("Reset").onActivate([&] {
    settings.path.screenshots = "";
    refreshPaths();
  });

  scriptAutoLoadLabel.setText("Auto-Load Script:");
  scriptAutoLoadPath.setEditable(false);
  scriptAutoLoadAssign.setText("Assign ...").onActivate([&] {
    if(auto location = program.selectPath()) {
      settings.script.autoLoadLocation = location;
      refreshPaths();
    }
  });
  scriptAutoLoadReset.setText("Reset").onActivate([&] {
    settings.script.autoLoadLocation = "";
    refreshPaths();
  });

  refreshPaths();
}

auto PathSettings::refreshPaths() -> void {
  if(auto location = settings.path.games) {
    gamesPath.setText(location).setForegroundColor();
  } else {
    gamesPath.setText("(last recently used)").setForegroundColor({128, 128, 128});
  }
  if(auto location = settings.path.patches) {
    patchesPath.setText(location).setForegroundColor();
  } else {
    patchesPath.setText("(same as loaded game)").setForegroundColor({128, 128, 128});
  }
  if(auto location = settings.path.saves) {
    savesPath.setText(location).setForegroundColor();
  } else {
    savesPath.setText("(same as loaded game)").setForegroundColor({128, 128, 128});
  }
  if(auto location = settings.path.cheats) {
    cheatsPath.setText(location).setForegroundColor();
  } else {
    cheatsPath.setText("(same as loaded game)").setForegroundColor({128, 128, 128});
  }
  if(auto location = settings.path.states) {
    statesPath.setText(location).setForegroundColor();
  } else {
    statesPath.setText("(same as loaded game)").setForegroundColor({128, 128, 128});
  }
  if(auto location = settings.path.screenshots) {
    screenshotsPath.setText(location).setForegroundColor();
  } else {
    screenshotsPath.setText("(same as loaded game)").setForegroundColor({128, 128, 128});
  }
  if(auto location = settings.script.autoLoadLocation) {
    scriptAutoLoadPath.setText(location).setForegroundColor();
  } else {
    scriptAutoLoadPath.setText("(none)").setForegroundColor({128, 128, 128});
  }
}
