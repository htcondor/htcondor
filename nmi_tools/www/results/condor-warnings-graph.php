<?php
  header("Content-Type: image/png");

  $args["branch"] = $_REQUEST["branch"];
  $args["warning_type"] = $_REQUEST["warning_type"]; # [total, unique]
  $args["platform"] = $_REQUEST["platform"]; # [platform, all, all,total, total]

  $branch = $args["branch"];
  $warning_type = $args["warning_type"];
  $platform = $args["platform"];

  # TODO - clean these up and provide a config  
  $base_dir = "/home/cndrauto/warnings";
  $script_location = "$base_dir/graph_warnings";

  $cmd = "$script_location $branch $platform $warning_type -";
  passthru($cmd);
?>
