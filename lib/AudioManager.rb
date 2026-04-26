class AudioManager
  def initialize
    @current_bgm = nil
    @current_file = nil
  end

  def play(file, volume = 0.8)
    return if file.nil? || file.empty?
    return if @current_file == file

    stop
    puts "Loading music: #{file}"   # отладочный вывод
    @current_bgm = Raylib.LoadMusicStream(file)
    @current_bgm.looping = true
    Raylib.SetMusicVolume(@current_bgm, volume.clamp(0.0, 1.0))
    Raylib.PlayMusicStream(@current_bgm)
    @current_file = file
  end

  def stop
    if @current_bgm
      Raylib.UnloadMusicStream(@current_bgm)
      @current_bgm = nil
      @current_file = nil
    end
  end

  def update
    if @current_bgm
      Raylib.UpdateMusicStream(@current_bgm)
    end
  end
end