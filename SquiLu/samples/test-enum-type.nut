{
  enum option_type { OPT_FLAG=1, OPT_INT};
  option_type opt = option_type.OPT_FLAG;
  print("opt", opt, option_type.OPT_INT);
  print(type(opt));
}
