import 'dart:typed_data';
import 'package:flutter/material.dart';
import 'package:win32audio/win32audio.dart';

/// A simple in-memory cache for Windows icons to prevent redundant native calls.
class IconCache {
  static final Map<String, Uint8List> _cache = <String, Uint8List>{};
  static final Map<String, Future<Uint8List?>> _pending = <String, Future<Uint8List?>>{};

  /// Retrieves an icon from the cache or fetches it natively if missing.
  /// Handles concurrent requests for the same icon by deduping futures.
  static Future<Uint8List?> getIcon(String path, {int iconID = 0}) async {
    final String key = "$path:$iconID";

    // 1. Return synchronously if already cached (fastest)
    if (_cache.containsKey(key)) {
      return _cache[key];
    }

    // 2. If a fetch is already in progress, join that future
    if (_pending.containsKey(key)) {
      return _pending[key];
    }

    // 3. Start a new fetch
    final Future<Uint8List?> future = WinIcons().extractFileIcon(path, iconID: iconID);
    _pending[key] = future;

    try {
      final Uint8List? data = await future;
      if (data != null && data.isNotEmpty) {
        _cache[key] = data;
      }
      return data;
    } catch (e) {
      debugPrint("Error fetching icon for $path: $e");
      return null;
    } finally {
      _pending.remove(key);
    }
  }

  static void clear() {
    _cache.clear();
    _pending.clear();
  }
}

class WinIconWidget extends StatefulWidget {
  final String path;
  final int iconID;
  final double width;
  final double height;

  const WinIconWidget({
    super.key,
    required this.path,
    this.iconID = 0,
    this.width = 32,
    this.height = 32,
  });

  @override
  State<WinIconWidget> createState() => _WinIconWidgetState();
}

class _WinIconWidgetState extends State<WinIconWidget> {
  late Future<Uint8List?> _iconFuture;

  @override
  void initState() {
    super.initState();
    _iconFuture = IconCache.getIcon(widget.path, iconID: widget.iconID);
  }

  @override
  void didUpdateWidget(covariant WinIconWidget oldWidget) {
    super.didUpdateWidget(oldWidget);
    if (widget.path != oldWidget.path || widget.iconID != oldWidget.iconID) {
      _iconFuture = IconCache.getIcon(widget.path, iconID: widget.iconID);
    }
  }

  @override
  void dispose() {
    IconCache.clear();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return FutureBuilder<Uint8List?>(
      future: _iconFuture,
      builder: (BuildContext context, AsyncSnapshot<Uint8List?> snapshot) {
        if (snapshot.connectionState == ConnectionState.done && snapshot.hasData) {
          return Image.memory(
            snapshot.data!,
            width: widget.width,
            height: widget.height,
            gaplessPlayback: true,
            filterQuality: FilterQuality.medium,
          );
        }
        return SizedBox(width: widget.width, height: widget.height, child: const Icon(Icons.spoke_outlined, color: Colors.grey));
      },
    );
  }
}
